#ifndef HTTP_STREAMER_H
#define HTTP_STREAMER_H

#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <opencv2/opencv.hpp>
#include <sys/socket.h>
#include <netinet/in.h>

class HTTPStreamer {
public:
    HTTPStreamer();
    ~HTTPStreamer();
    
    bool init(int port = 8080);
    void start();
    void stop();
    void updateFrame(const cv::Mat& frame);
    bool isRunning() const { return running_; }
    
private:
    void serverThread();
    void handleClient(int client_socket);
    
    int port_;
    int server_socket_;
    std::atomic<bool> running_;
    std::thread server_thread_;
    
    cv::Mat current_frame_;
    std::mutex frame_mutex_;
};

#endif // HTTP_STREAMER_H
