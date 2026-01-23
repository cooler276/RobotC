/**
 * @file led_controller.h
 * @brief LED control via PWM
 * @author RobotC Project
 * @date 2026-01-23
 */
#ifndef LED_CONTROLLER_H
#define LED_CONTROLLER_H

#include <cstdint>
#include <string>

class LEDController {
public:
    LEDController();
    ~LEDController();
    
    bool init();
    void close();
    
    bool setBrightness(uint8_t led1, uint8_t led2);  // 0-255
    bool blink(uint8_t brightness = 255, int times = 3, int interval_ms = 200);
    bool off();
    
private:
    static constexpr int PWM_CHIP = 0;
    static constexpr int PWM_CHANNEL_1 = 0;  // GPIO12
    static constexpr int PWM_CHANNEL_2 = 1;  // GPIO13
    static constexpr int PWM_PERIOD_NS = 1000000;  // 1ms (1kHz)
    
    bool setPWM(int channel, uint8_t brightness);
    bool exportPWM(int channel);
    bool setPeriod(int channel, int period_ns);
    bool setDutyCycle(int channel, int duty_ns);
    bool enable(int channel, bool enable);
    
    bool initialized_;
};

#endif // LED_CONTROLLER_H
