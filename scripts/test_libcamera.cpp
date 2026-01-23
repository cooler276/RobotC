#include <libcamera/libcamera.h>
#include <opencv2/opencv.hpp>
#include <iostream>
#include <memory>

using namespace libcamera;
using namespace cv;
using namespace std;

class LibCameraCapture {
public:
    LibCameraCapture() : camera_manager_(make_unique<CameraManager>()) {
        camera_manager_->start();
        
        if (camera_manager_->cameras().empty()) {
            throw runtime_error("No cameras available");
        }
        
        camera_ = camera_manager_->cameras()[0];
        camera_->acquire();
        
        // カメラ設定
        config_ = camera_->generateConfiguration({StreamRole::Viewfinder});
        StreamConfiguration &cfg = config_->at(0);
        cfg.size.width = 320;
        cfg.size.height = 240;
        cfg.pixelFormat = formats::RGB888;
        
        config_->validate();
        camera_->configure(config_.get());
        
        // バッファ割り当て
        allocator_ = make_unique<FrameBufferAllocator>(camera_);
        Stream *stream = cfg.stream();
        allocator_->allocate(stream);
        
        // リクエスト作成
        for (const unique_ptr<FrameBuffer> &buffer : allocator_->buffers(stream)) {
            unique_ptr<Request> request = camera_->createRequest();
            request->addBuffer(stream, buffer.get());
            requests_.push_back(move(request));
        }
        
        camera_->start();
        
        for (unique_ptr<Request> &request : requests_) {
            camera_->queueRequest(request.get());
        }
    }
    
    ~LibCameraCapture() {
        camera_->stop();
        allocator_->free(config_->at(0).stream());
        camera_->release();
        camera_manager_->stop();
    }
    
    bool read(Mat &frame) {
        unique_ptr<Request> request = camera_->requestCompleted.get();
        
        if (!request)
            return false;
        
        FrameBuffer *buffer = request->buffers().begin()->second;
        const FrameBuffer::Plane &plane = buffer->planes()[0];
        
        // RGB888データをMatに変換
        uint8_t *data = static_cast<uint8_t*>(mmap(nullptr, plane.length,
                                                     PROT_READ, MAP_SHARED,
                                                     plane.fd.get(), 0));
        
        if (data == MAP_FAILED)
            return false;
        
        frame = Mat(240, 320, CV_8UC3, data).clone();
        
        munmap(data, plane.length);
        
        // リクエストを再キュー
        request->reuse(Request::ReuseBuffers);
        camera_->queueRequest(request.get());
        
        return true;
    }
    
private:
    unique_ptr<CameraManager> camera_manager_;
    shared_ptr<Camera> camera_;
    unique_ptr<CameraConfiguration> config_;
    unique_ptr<FrameBufferAllocator> allocator_;
    vector<unique_ptr<Request>> requests_;
};

int main() {
    try {
        LibCameraCapture cap;
        
        cout << "Capturing frames..." << endl;
        
        for (int i = 0; i < 10; i++) {
            Mat frame;
            if (cap.read(frame)) {
                cout << "Frame " << i << ": " << frame.cols << "x" << frame.rows << endl;
            } else {
                cerr << "Failed to capture frame " << i << endl;
            }
        }
        
        cout << "Test completed successfully!" << endl;
        
    } catch (const exception &e) {
        cerr << "Error: " << e.what() << endl;
        return -1;
    }
    
    return 0;
}
