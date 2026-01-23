#include "camera/libcamera_capture.h"
#include <sys/mman.h>
#include <iostream>

using namespace libcamera;
using namespace cv;
using namespace std;

LibCameraCapture::LibCameraCapture(int width, int height)
    : width_(width), height_(height), stream_(nullptr), 
      completed_request_(nullptr), running_(false) {
    
    camera_manager_ = make_unique<CameraManager>();
    int ret = camera_manager_->start();
    
    if (ret) {
        cerr << "[LibCamera] Failed to start camera manager" << endl;
        return;
    }
    
    if (camera_manager_->cameras().empty()) {
        cerr << "[LibCamera] No cameras available" << endl;
        return;
    }
    
    // 最初のカメラを使用
    camera_ = camera_manager_->cameras()[0];
    ret = camera_->acquire();
    
    if (ret) {
        cerr << "[LibCamera] Failed to acquire camera" << endl;
        camera_.reset();
        return;
    }
    
    // カメラ設定を生成
    config_ = camera_->generateConfiguration({StreamRole::Viewfinder});
    
    if (!config_) {
        cerr << "[LibCamera] Failed to generate configuration" << endl;
        camera_->release();
        camera_.reset();
        return;
    }
    
    StreamConfiguration &cfg = config_->at(0);
    cfg.size.width = width_;
    cfg.size.height = height_;
    cfg.pixelFormat = formats::RGB888;
    cfg.bufferCount = 4; // バッファ数を設定
    
    CameraConfiguration::Status status = config_->validate();
    
    if (status == CameraConfiguration::Invalid) {
        cerr << "[LibCamera] Configuration invalid" << endl;
        camera_->release();
        camera_.reset();
        return;
    }
    
    ret = camera_->configure(config_.get());
    
    if (ret) {
        cerr << "[LibCamera] Failed to configure camera" << endl;
        camera_->release();
        camera_.reset();
        return;
    }
    
    stream_ = cfg.stream();
    
    // フレームバッファを割り当て
    allocator_ = make_unique<FrameBufferAllocator>(camera_);
    ret = allocator_->allocate(stream_);
    
    if (ret < 0) {
        cerr << "[LibCamera] Failed to allocate buffers" << endl;
        camera_->release();
        camera_.reset();
        return;
    }
    
    // リクエストを作成
    for (const unique_ptr<FrameBuffer> &buffer : allocator_->buffers(stream_)) {
        unique_ptr<Request> request = camera_->createRequest();
        
        if (!request) {
            cerr << "[LibCamera] Failed to create request" << endl;
            allocator_->free(stream_);
            camera_->release();
            camera_.reset();
            return;
        }
        
        ret = request->addBuffer(stream_, buffer.get());
        
        if (ret < 0) {
            cerr << "[LibCamera] Failed to add buffer to request" << endl;
            allocator_->free(stream_);
            camera_->release();
            camera_.reset();
            return;
        }
        
        requests_.push_back(move(request));
    }
    
    // リクエスト完了コールバックを設定
    camera_->requestCompleted.connect(this, &LibCameraCapture::requestComplete);
    
    // カメラを起動
    ret = camera_->start();
    
    if (ret) {
        cerr << "[LibCamera] Failed to start camera" << endl;
        allocator_->free(stream_);
        camera_->release();
        camera_.reset();
        return;
    }
    
    running_ = true;
    
    // リクエストをキュー
    for (unique_ptr<Request> &request : requests_) {
        ret = camera_->queueRequest(request.get());
        
        if (ret < 0) {
            cerr << "[LibCamera] Failed to queue request" << endl;
        }
    }
    
    cout << "[LibCamera] Camera initialized (" << width_ << "x" << height_ << ")" << endl;
}

LibCameraCapture::~LibCameraCapture() {
    release();
}

void LibCameraCapture::release() {
    if (!camera_)
        return;
    
    running_ = false;
    cv_.notify_all();
    
    camera_->stop();
    allocator_->free(stream_);
    camera_->release();
    camera_.reset();
    camera_manager_->stop();
}

void LibCameraCapture::requestComplete(Request *request) {
    if (request->status() == Request::RequestCancelled)
        return;
    
    {
        lock_guard<mutex> lock(mutex_);
        completed_request_ = request;
    }
    cv_.notify_one();
}

bool LibCameraCapture::read(Mat &frame) {
    if (!camera_ || !running_)
        return false;
    
    Request *request = nullptr;
    
    {
        unique_lock<mutex> lock(mutex_);
        cv_.wait_for(lock, chrono::milliseconds(1000), [this] {
            return completed_request_ != nullptr || !running_;
        });
        
        if (!running_ || !completed_request_)
            return false;
        
        request = completed_request_;
        completed_request_ = nullptr;
    }
    
    // フレームバッファからデータを取得
    FrameBuffer *buffer = request->buffers().at(stream_);
    const FrameBuffer::Plane &plane = buffer->planes()[0];
    
    // メモリマップ
    void *data = mmap(nullptr, plane.length, PROT_READ, MAP_SHARED,
                      plane.fd.get(), 0);
    
    if (data == MAP_FAILED) {
        cerr << "[LibCamera] Failed to mmap buffer" << endl;
        request->reuse(Request::ReuseBuffers);
        camera_->queueRequest(request);
        return false;
    }
    
    // RGB888データをMatに変換（コピー）
    Mat temp(height_, width_, CV_8UC3, data);
    frame = temp.clone();
    
    // メモリマップ解放
    munmap(data, plane.length);
    
    // リクエストを再利用してキュー
    request->reuse(Request::ReuseBuffers);
    camera_->queueRequest(request);
    
    return !frame.empty();
}
