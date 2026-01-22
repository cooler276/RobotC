#ifndef MOTOR_DRIVER_H
#define MOTOR_DRIVER_H

#include "pico/stdlib.h"
#include "pico/types.h"
#include "hardware/pwm.h"

class MotorDriver {
public:
    enum Motor {
        MOTOR_ROTATION = 0,  // M1 (回転)
        MOTOR_DRIVE = 1      // M2 (前進/後退)
    };
    
    MotorDriver();
    bool init();
    void setSpeed(Motor motor, int8_t speed);
    void stop();
    
private:
    // spec.mdに従ったピン配置
    static constexpr uint MOTOR1_PIN_A = 8;   // GPIO8  (PWM_6A)
    static constexpr uint MOTOR1_PIN_B = 9;   // GPIO9  (PWM_6B)
    static constexpr uint MOTOR2_PIN_A = 13;  // GPIO13 (PWM_4A)
    static constexpr uint MOTOR2_PIN_B = 12;  // GPIO12 (PWM_4B)
    
    static constexpr uint PWM_FREQ = 1000;
    static constexpr uint16_t PWM_MAX = 65535;
    
    uint slice_m1;
    uint slice_m2;
};

#endif // MOTOR_DRIVER_H
