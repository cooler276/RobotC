/**
 * @file led_controller.cpp
 * @brief Implementation of LED control
 * @author RobotC Project
 * @date 2026-01-23
 */

#include "hardware/led_controller.h"
#include <iostream>
#include <fstream>
#include <thread>
#include <chrono>
#include <cstring>

LEDController::LEDController() : initialized_(false) {}

LEDController::~LEDController() {
    close();
}

bool LEDController::init() {
    // PWMチャンネルをexport
    if (!exportPWM(PWM_CHANNEL_1) || !exportPWM(PWM_CHANNEL_2)) {
        std::cerr << "Failed to export PWM channels" << std::endl;
        return false;
    }
    
    // period設定
    if (!setPeriod(PWM_CHANNEL_1, PWM_PERIOD_NS) || 
        !setPeriod(PWM_CHANNEL_2, PWM_PERIOD_NS)) {
        std::cerr << "Failed to set PWM period" << std::endl;
        return false;
    }
    
    // 初期化時はOFF
    off();
    
    // 有効化
    if (!enable(PWM_CHANNEL_1, true) || !enable(PWM_CHANNEL_2, true)) {
        std::cerr << "Failed to enable PWM channels" << std::endl;
        return false;
    }
    
    initialized_ = true;
    std::cout << "LED controller initialized (PWM GPIO12/13)" << std::endl;
    return true;
}

void LEDController::close() {
    if (initialized_) {
        off();
        enable(PWM_CHANNEL_1, false);
        enable(PWM_CHANNEL_2, false);
        initialized_ = false;
    }
}

bool LEDController::exportPWM(int channel) {
    std::string path = "/sys/class/pwm/pwmchip" + std::to_string(PWM_CHIP) + "/export";
    std::ofstream export_file(path);
    if (!export_file.is_open()) {
        // 既にエクスポート済みの可能性
        return true;
    }
    export_file << channel;
    export_file.close();
    
    // エクスポート後の待機
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    return true;
}

bool LEDController::setPeriod(int channel, int period_ns) {
    std::string path = "/sys/class/pwm/pwmchip" + std::to_string(PWM_CHIP) + 
                       "/pwm" + std::to_string(channel) + "/period";
    std::ofstream period_file(path);
    if (!period_file.is_open()) {
        std::cerr << "Failed to open PWM period file: " << path << std::endl;
        return false;
    }
    period_file << period_ns;
    return true;
}

bool LEDController::setDutyCycle(int channel, int duty_ns) {
    std::string path = "/sys/class/pwm/pwmchip" + std::to_string(PWM_CHIP) + 
                       "/pwm" + std::to_string(channel) + "/duty_cycle";
    std::ofstream duty_file(path);
    if (!duty_file.is_open()) {
        std::cerr << "Failed to open PWM duty_cycle file: " << path << std::endl;
        return false;
    }
    duty_file << duty_ns;
    return true;
}

bool LEDController::enable(int channel, bool enable) {
    std::string path = "/sys/class/pwm/pwmchip" + std::to_string(PWM_CHIP) + 
                       "/pwm" + std::to_string(channel) + "/enable";
    std::ofstream enable_file(path);
    if (!enable_file.is_open()) {
        std::cerr << "Failed to open PWM enable file: " << path << std::endl;
        return false;
    }
    enable_file << (enable ? "1" : "0");
    return true;
}

bool LEDController::setPWM(int channel, uint8_t brightness) {
    // brightness: 0-255 → duty_cycle: 0-PWM_PERIOD_NS
    int duty_ns = (brightness * PWM_PERIOD_NS) / 255;
    return setDutyCycle(channel, duty_ns);
}

bool LEDController::setBrightness(uint8_t led1, uint8_t led2) {
    if (!initialized_) return false;
    return setPWM(PWM_CHANNEL_1, led1) && setPWM(PWM_CHANNEL_2, led2);
}

bool LEDController::blink(uint8_t brightness, int times, int interval_ms) {
    for (int i = 0; i < times; i++) {
        setBrightness(brightness, brightness);
        std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms));
        off();
        std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms));
    }
    return true;
}

bool LEDController::off() {
    return setBrightness(0, 0);
}
