#include "http_streamer.h"
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>

HTTPStreamer::HTTPStreamer() : port_(8080), server_socket_(-1), running_(false) {}

HTTPStreamer::~HTTPStreamer() {
    stop();
}

bool HTTPStreamer::init(int port) {
    port_ = port;
    
    server_socket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket_ < 0) {
        std::cerr << "[HTTPStreamer] Failed to create socket" << std::endl;
        return false;
    }
    
    int opt = 1;
    setsockopt(server_socket_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port_);
    
    if (bind(server_socket_, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        std::cerr << "[HTTPStreamer] Failed to bind socket to port " << port_ << std::endl;
        close(server_socket_);
        server_socket_ = -1;
        return false;
    }
    
    if (listen(server_socket_, 5) < 0) {
        std::cerr << "[HTTPStreamer] Failed to listen on socket" << std::endl;
        close(server_socket_);
        server_socket_ = -1;
        return false;
    }
    
    std::cout << "[HTTPStreamer] HTTP server initialized on port " << port_ << std::endl;
    return true;
}

void HTTPStreamer::start() {
    if (running_) {
        return;
    }
    
    running_ = true;
    server_thread_ = std::thread(&HTTPStreamer::serverThread, this);
    std::cout << "[HTTPStreamer] Server thread started" << std::endl;
}

void HTTPStreamer::stop() {
    if (!running_) {
        return;
    }
    
    running_ = false;
    
    if (server_socket_ >= 0) {
        close(server_socket_);
        server_socket_ = -1;
    }
    
    if (server_thread_.joinable()) {
        server_thread_.join();
    }
    
    std::cout << "[HTTPStreamer] Server stopped" << std::endl;
}

void HTTPStreamer::updateFrame(const cv::Mat& frame) {
    if (frame.empty()) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(frame_mutex_);
    current_frame_ = frame.clone();
}

void HTTPStreamer::serverThread() {
    while (running_) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        int client_socket = accept(server_socket_, (struct sockaddr*)&client_addr, &client_len);
        if (client_socket < 0) {
            if (running_) {
                std::cerr << "[HTTPStreamer] Accept failed" << std::endl;
            }
            continue;
        }
        
        std::cout << "[HTTPStreamer] Client connected from " 
                  << inet_ntoa(client_addr.sin_addr) << std::endl;
        
        // クライアント処理を別スレッドで実行
        std::thread([this, client_socket]() {
            handleClient(client_socket);
        }).detach();
    }
}

void HTTPStreamer::handleClient(int client_socket) {
    // HTTPヘッダー送信
    const char* header = 
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n"
        "Cache-Control: no-cache\r\n"
        "Connection: close\r\n"
        "\r\n";
    
    send(client_socket, header, strlen(header), 0);
    
    while (running_) {
        cv::Mat frame;
        {
            std::lock_guard<std::mutex> lock(frame_mutex_);
            if (current_frame_.empty()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(33));
                continue;
            }
            frame = current_frame_.clone();
        }
        
        // JPEGエンコード（メモリ節約のため低品質）
        std::vector<uchar> buffer;
        std::vector<int> params = {cv::IMWRITE_JPEG_QUALITY, 50};
        if (!cv::imencode(".jpg", frame, buffer, params)) {
            std::cerr << "[HTTPStreamer] Failed to encode frame" << std::endl;
            break;
        }
        
        // MJPEGフレーム送信
        std::string frame_header = 
            "--frame\r\n"
            "Content-Type: image/jpeg\r\n"
            "Content-Length: " + std::to_string(buffer.size()) + "\r\n"
            "\r\n";
        
        if (send(client_socket, frame_header.c_str(), frame_header.size(), 0) < 0) {
            break;
        }
        
        if (send(client_socket, buffer.data(), buffer.size(), 0) < 0) {
            break;
        }
        
        const char* frame_end = "\r\n";
        if (send(client_socket, frame_end, strlen(frame_end), 0) < 0) {
            break;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100)); // ~10fps（メモリ節約）
    }
    
    close(client_socket);
    std::cout << "[HTTPStreamer] Client disconnected" << std::endl;
}
