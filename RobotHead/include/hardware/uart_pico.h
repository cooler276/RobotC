/**
 * @file uart_pico.h
 * @brief UART communication with Raspberry Pi Pico
 * @author RobotC Project
 * @date 2026-01-23
 */

#ifndef UART_PICO_H
#define UART_PICO_H

#include <string>
#include <functional>
#include <vector>

class UARTPico {
public:
    UARTPico();
    ~UARTPico();
    
    bool init(const std::string& device, int baudrate = 115200);
    void close();
    
    bool sendCommand(const std::string& cmd);
    void update();  // 定期呼び出し用（受信処理）
    
    // コールバック設定
    void setIMUCallback(std::function<void(float ax, float ay, float az, float gx, float gy, float gz)> callback);
    void setAlertCallback(std::function<void(const std::string& reason)> callback);
    void setInfoCallback(std::function<void(const std::string& info)> callback);
    
    // コマンド送信ヘルパー
    bool setRotation(int8_t speed);  // -100 ~ 100
    bool setDrive(int8_t speed);     // -100 ~ 100
    bool stop();
    bool calibrate();
    bool reset();
    
private:
    void processLine(const std::string& line);
    
    int fd_;
    std::string read_buffer_;
    
    std::function<void(float, float, float, float, float, float)> imu_callback_;
    std::function<void(const std::string&)> alert_callback_;
    std::function<void(const std::string&)> info_callback_;
};

#endif // UART_PICO_H
