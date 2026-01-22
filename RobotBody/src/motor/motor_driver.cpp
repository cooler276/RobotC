#include "motor_driver.h"
#include <cstdlib>

MotorDriver::MotorDriver() : slice_m1(0), slice_m2(0) {}

bool MotorDriver::init() {
    // GPIO8, GPIO9 (Motor1 - 回転) の設定
    gpio_set_function(MOTOR1_PIN_A, GPIO_FUNC_PWM);
    gpio_set_function(MOTOR1_PIN_B, GPIO_FUNC_PWM);
    
    // GPIO12, GPIO13 (Motor2 - 前進/後退) の設定
    gpio_set_function(MOTOR2_PIN_A, GPIO_FUNC_PWM);
    gpio_set_function(MOTOR2_PIN_B, GPIO_FUNC_PWM);
    
    // PWMスライス取得
    slice_m1 = pwm_gpio_to_slice_num(MOTOR1_PIN_A);
    slice_m2 = pwm_gpio_to_slice_num(MOTOR2_PIN_A);
    
    // PWM設定 (1kHz)
    uint32_t clock_freq = 125000000;  // 125MHz
    uint32_t divider = clock_freq / (PWM_FREQ * PWM_MAX);
    
    pwm_set_clkdiv(slice_m1, divider);
    pwm_set_wrap(slice_m1, PWM_MAX - 1);
    pwm_set_enabled(slice_m1, true);
    
    pwm_set_clkdiv(slice_m2, divider);
    pwm_set_wrap(slice_m2, PWM_MAX - 1);
    pwm_set_enabled(slice_m2, true);
    
    // 初期状態: 停止
    stop();
    
    return true;
}

void MotorDriver::setSpeed(Motor motor, int8_t speed) {
    // speed: -100 (逆転最大) ～ 0 (停止) ～ +100 (正転最大)
    if (speed < -100) speed = -100;
    if (speed > 100) speed = 100;
    
    // デッドバンド補正（モーターの最小起動トルク対応）
    // 回転モーターと駆動モーターで負荷が異なるため別々に設定
    const int8_t MIN_SPEED_ROTATION = 20;  // 回転モーター用
    const int8_t MIN_SPEED_DRIVE = 65;     // 駆動モーター用（高負荷）
    
    int8_t min_speed = (motor == MOTOR_ROTATION) ? MIN_SPEED_ROTATION : MIN_SPEED_DRIVE;
    uint16_t pwm_value;
    
    if (abs(speed) > 0 && abs(speed) < min_speed) {
        // MIN_SPEED未満の場合はMIN_SPEEDにマッピング
        pwm_value = (min_speed * PWM_MAX) / 100;
    } else if (speed == 0) {
        pwm_value = 0;
    } else {
        pwm_value = (abs(speed) * PWM_MAX) / 100;
    }
    
    if (motor == MOTOR_ROTATION) {
        // Motor1 (回転)
        if (speed >= 0) {
            pwm_set_gpio_level(MOTOR1_PIN_A, pwm_value);
            pwm_set_gpio_level(MOTOR1_PIN_B, 0);
        } else {
            pwm_set_gpio_level(MOTOR1_PIN_A, 0);
            pwm_set_gpio_level(MOTOR1_PIN_B, pwm_value);
        }
    } else {
        // Motor2 (前進/後退)
        if (speed >= 0) {
            pwm_set_gpio_level(MOTOR2_PIN_A, pwm_value);
            pwm_set_gpio_level(MOTOR2_PIN_B, 0);
        } else {
            pwm_set_gpio_level(MOTOR2_PIN_A, 0);
            pwm_set_gpio_level(MOTOR2_PIN_B, pwm_value);
        }
    }
}

void MotorDriver::stop() {
    pwm_set_gpio_level(MOTOR1_PIN_A, 0);
    pwm_set_gpio_level(MOTOR1_PIN_B, 0);
    pwm_set_gpio_level(MOTOR2_PIN_A, 0);
    pwm_set_gpio_level(MOTOR2_PIN_B, 0);
}
