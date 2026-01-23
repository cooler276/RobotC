/**
 * @file uart_pico.cpp
 * @brief Implementation of UART communication with Pico
 * @author RobotC Project
 * @date 2026-01-23
 */

#include "hardware/uart_pico.h"
#include <iostream>
#include <sstream>
#include <cstring>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

UARTPico::UARTPico() : fd_(-1) {}

UARTPico::~UARTPico() {
    close();
}

bool UARTPico::init(const std::string& device, int baudrate) {
    fd_ = open(device.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd_ < 0) {
        std::cerr << "Failed to open UART device: " << device << std::endl;
        return false;
    }
    
    struct termios tty;
    if (tcgetattr(fd_, &tty) != 0) {
        std::cerr << "Failed to get UART attributes" << std::endl;
        ::close(fd_);
        fd_ = -1;
        return false;
    }
    
    // ボーレート設定
    speed_t speed = B115200;
    if (baudrate == 9600) speed = B9600;
    else if (baudrate == 19200) speed = B19200;
    else if (baudrate == 38400) speed = B38400;
    else if (baudrate == 57600) speed = B57600;
    else if (baudrate == 115200) speed = B115200;
    
    cfsetospeed(&tty, speed);
    cfsetispeed(&tty, speed);
    
    // 8N1設定
    tty.c_cflag &= ~PARENB;
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;
    tty.c_cflag &= ~CRTSCTS;
    tty.c_cflag |= CREAD | CLOCAL;
    
    tty.c_lflag &= ~ICANON;
    tty.c_lflag &= ~ECHO;
    tty.c_lflag &= ~ECHOE;
    tty.c_lflag &= ~ECHONL;
    tty.c_lflag &= ~ISIG;
    
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);
    tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL);
    
    tty.c_oflag &= ~OPOST;
    tty.c_oflag &= ~ONLCR;
    
    tty.c_cc[VTIME] = 0;
    tty.c_cc[VMIN] = 0;
    
    if (tcsetattr(fd_, TCSANOW, &tty) != 0) {
        std::cerr << "Failed to set UART attributes" << std::endl;
        ::close(fd_);
        fd_ = -1;
        return false;
    }
    
    std::cout << "UART initialized: " << device << std::endl;
    return true;
}

void UARTPico::close() {
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}

bool UARTPico::sendCommand(const std::string& cmd) {
    if (fd_ < 0) return false;
    
    std::string full_cmd = cmd + "\n";
    ssize_t written = write(fd_, full_cmd.c_str(), full_cmd.length());
    return (written == (ssize_t)full_cmd.length());
}

void UARTPico::update() {
    if (fd_ < 0) return;
    
    char buffer[256];
    ssize_t n = read(fd_, buffer, sizeof(buffer) - 1);
    
    if (n > 0) {
        buffer[n] = '\0';
        read_buffer_ += buffer;
        
        // 行ごとに処理
        size_t pos;
        while ((pos = read_buffer_.find('\n')) != std::string::npos) {
            std::string line = read_buffer_.substr(0, pos);
            read_buffer_.erase(0, pos + 1);
            
            if (!line.empty()) {
                processLine(line);
            }
        }
    }
}

void UARTPico::processLine(const std::string& line) {
    std::istringstream iss(line);
    std::string token;
    std::getline(iss, token, ',');
    
    if (token == "IMU") {
        // IMU,ax,ay,az,gx,gy,gz
        float ax, ay, az, gx, gy, gz;
        if (iss >> ax && iss.ignore(1) && iss >> ay && iss.ignore(1) && 
            iss >> az && iss.ignore(1) && iss >> gx && iss.ignore(1) && 
            iss >> gy && iss.ignore(1) && iss >> gz) {
            if (imu_callback_) {
                imu_callback_(ax, ay, az, gx, gy, gz);
            }
        }
    } else if (token == "ALERT") {
        // ALERT,reason
        std::string reason;
        std::getline(iss, reason, ',');
        if (alert_callback_) {
            alert_callback_(reason);
        }
    } else if (token == "ACK") {
        // ACK,command
        std::string cmd;
        std::getline(iss, cmd);
        std::cout << "ACK: " << cmd << std::endl;
    } else if (token == "NG") {
        // NG,command,reason
        std::string cmd, reason;
        std::getline(iss, cmd, ',');
        std::getline(iss, reason);
        std::cerr << "NG: " << cmd << " (" << reason << ")" << std::endl;
    } else if (token == "INFO") {
        // INFO,version
        std::string info;
        std::getline(iss, info);
        if (info_callback_) {
            info_callback_(info);
        }
    }
}

void UARTPico::setIMUCallback(std::function<void(float, float, float, float, float, float)> callback) {
    imu_callback_ = callback;
}

void UARTPico::setAlertCallback(std::function<void(const std::string&)> callback) {
    alert_callback_ = callback;
}

void UARTPico::setInfoCallback(std::function<void(const std::string&)> callback) {
    info_callback_ = callback;
}

bool UARTPico::setRotation(int8_t speed) {
    if (speed < -100) speed = -100;
    if (speed > 100) speed = 100;
    return sendCommand("R" + std::to_string(speed));
}

bool UARTPico::setDrive(int8_t speed) {
    if (speed < -100) speed = -100;
    if (speed > 100) speed = 100;
    return sendCommand("D" + std::to_string(speed));
}

bool UARTPico::stop() {
    return sendCommand("S");
}

bool UARTPico::calibrate() {
    return sendCommand("CALIB");
}

bool UARTPico::reset() {
    return sendCommand("RESET");
}
