#include "motor_driver.h"
#include <cstdlib>

MotorDriver::MotorDriver() 
    : slice_m1(0), slice_m2(0),
      current_rotation_speed(0), current_drive_speed(0),
      target_rotation_speed(0), target_drive_speed(0),
      last_update_time(0), boost_start_time(0), boost_active(false) {}

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
    
    // 回転と前後移動の同時動作防止（バッテリー負荷対策）
    if (motor == MOTOR_ROTATION) {
        target_rotation_speed = speed;
        if (speed != 0 && current_drive_speed != 0) {
            // 駆動中の場合は駆動を停止
            target_drive_speed = 0;
            current_drive_speed = 0;
            applyPWM(MOTOR_DRIVE, 0);
        }
    } else {
        target_drive_speed = speed;
        if (speed != 0 && current_rotation_speed != 0) {
            // 回転中の場合は回転を停止
            target_rotation_speed = 0;
            current_rotation_speed = 0;
            applyPWM(MOTOR_ROTATION, 0);
        }
        
        // 前進後退時：停止状態からの起動時はブースト有効化
        if (current_drive_speed == 0 && speed != 0) {
            boost_active = true;
            boost_start_time = to_ms_since_boot(get_absolute_time());
        }
    }
}

void MotorDriver::stop() {
    target_rotation_speed = 0;
    target_drive_speed = 0;
    current_rotation_speed = 0;
    current_drive_speed = 0;
    boost_active = false;
    
    pwm_set_gpio_level(MOTOR1_PIN_A, 0);
    pwm_set_gpio_level(MOTOR1_PIN_B, 0);
    pwm_set_gpio_level(MOTOR2_PIN_A, 0);
    pwm_set_gpio_level(MOTOR2_PIN_B, 0);
}

// PWM出力（デッドバンド補正込み）
void MotorDriver::applyPWM(Motor motor, int8_t speed) {
    // デッドバンド補正（モーターの最小起動トルク対応）
    const int8_t MIN_SPEED_ROTATION = 20;  // 回転モーター用
    const int8_t MIN_SPEED_DRIVE = 65;     // 駆動モーター用（高負荷）
    
    int8_t min_speed = (motor == MOTOR_ROTATION) ? MIN_SPEED_ROTATION : MIN_SPEED_DRIVE;
    uint16_t pwm_value;
    
    if (abs(speed) > 0 && abs(speed) < min_speed) {
        pwm_value = (min_speed * PWM_MAX) / 100;
    } else if (speed == 0) {
        pwm_value = 0;
    } else {
        pwm_value = (abs(speed) * PWM_MAX) / 100;
    }
    
    if (motor == MOTOR_ROTATION) {
        if (speed >= 0) {
            pwm_set_gpio_level(MOTOR1_PIN_A, pwm_value);
            pwm_set_gpio_level(MOTOR1_PIN_B, 0);
        } else {
            pwm_set_gpio_level(MOTOR1_PIN_A, 0);
            pwm_set_gpio_level(MOTOR1_PIN_B, pwm_value);
        }
    } else {
        if (speed >= 0) {
            pwm_set_gpio_level(MOTOR2_PIN_A, pwm_value);
            pwm_set_gpio_level(MOTOR2_PIN_B, 0);
        } else {
            pwm_set_gpio_level(MOTOR2_PIN_A, 0);
            pwm_set_gpio_level(MOTOR2_PIN_B, pwm_value);
        }
    }
}

// 定期更新（ランプ・ブースト制御）
void MotorDriver::update() {
    uint64_t now = to_ms_since_boot(get_absolute_time());
    
    // 初回呼び出し時の初期化
    if (last_update_time == 0) {
        last_update_time = now;
        return;
    }
    
    // 更新間隔チェック
    if (now - last_update_time < RAMP_STEP_MS) {
        return;
    }
    last_update_time = now;
    
    // 回転モーター：ランプ制御（徐々に加速）
    if (current_rotation_speed != target_rotation_speed) {
        if (abs(current_rotation_speed - target_rotation_speed) <= RAMP_SPEED_STEP) {
            current_rotation_speed = target_rotation_speed;
        } else if (current_rotation_speed < target_rotation_speed) {
            current_rotation_speed += RAMP_SPEED_STEP;
        } else {
            current_rotation_speed -= RAMP_SPEED_STEP;
        }
        applyPWM(MOTOR_ROTATION, current_rotation_speed);
    }
    
    // 駆動モーター：ブースト制御（100%→目標速度）
    if (boost_active) {
        uint64_t boost_elapsed = now - boost_start_time;
        if (boost_elapsed >= BOOST_DURATION_MS) {
            // ブースト終了：目標速度へ
            boost_active = false;
            current_drive_speed = target_drive_speed;
            applyPWM(MOTOR_DRIVE, current_drive_speed);
        } else {
            // ブースト中：100%で駆動
            current_drive_speed = (target_drive_speed >= 0) ? BOOST_SPEED : -BOOST_SPEED;
            applyPWM(MOTOR_DRIVE, current_drive_speed);
        }
    } else if (current_drive_speed != target_drive_speed) {
        // 通常時：即座に目標速度へ
        current_drive_speed = target_drive_speed;
        applyPWM(MOTOR_DRIVE, current_drive_speed);
    }
}
