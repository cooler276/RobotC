#ifndef LIBCAMERA_CAPTURE_H
#define LIBCAMERA_CAPTURE_H

#include <libcamera/libcamera.h>
#include <opencv2/opencv.hpp>
#include <memory>
#include <mutex>
#include <condition_variable>

class LibCameraCapture {
public:
    LibCameraCapture(int width = 320, int height = 240);
    ~LibCameraCapture();
    
    bool isOpened() const { return camera_ != nullptr; }
    bool read(cv::Mat &frame);
    void release();
    
private:
    void requestComplete(libcamera::Request *request);
    
    std::unique_ptr<libcamera::CameraManager> camera_manager_;
    std::shared_ptr<libcamera::Camera> camera_;
    std::unique_ptr<libcamera::CameraConfiguration> config_;
    std::unique_ptr<libcamera::FrameBufferAllocator> allocator_;
    std::vector<std::unique_ptr<libcamera::Request>> requests_;
    
    libcamera::Stream *stream_;
    int width_;
    int height_;
    
    std::mutex mutex_;
    std::condition_variable cv_;
    libcamera::Request *completed_request_;
    bool running_;
};

#endif // LIBCAMERA_CAPTURE_H
