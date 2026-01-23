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
    void update();  // 定期呼び出し用（ランプ・ブースト制御）
    
private:
    // spec.mdに従ったピン配置
    static constexpr uint MOTOR1_PIN_A = 8;   // GPIO8  (PWM_6A)
    static constexpr uint MOTOR1_PIN_B = 9;   // GPIO9  (PWM_6B)
    static constexpr uint MOTOR2_PIN_A = 13;  // GPIO13 (PWM_4A)
    static constexpr uint MOTOR2_PIN_B = 12;  // GPIO12 (PWM_4B)
    
    static constexpr uint PWM_FREQ = 1000;
    static constexpr uint16_t PWM_MAX = 65535;
    
    // ランプ・ブースト制御パラメータ
    static constexpr uint RAMP_STEP_MS = 50;       // ランプ更新間隔（ms）
    static constexpr int8_t RAMP_SPEED_STEP = 5;   // 回転時の加速ステップ
    static constexpr uint BOOST_DURATION_MS = 200; // ブースト持続時間（ms）
    static constexpr int8_t BOOST_SPEED = 100;     // ブースト速度（100%）
    
    uint slice_m1;
    uint slice_m2;
    
    // モーター状態管理
    int8_t current_rotation_speed;     // 現在の回転速度
    int8_t current_drive_speed;        // 現在の駆動速度
    int8_t target_rotation_speed;      // 目標回転速度
    int8_t target_drive_speed;         // 目標駆動速度
    
    uint64_t last_update_time;         // 最終更新時刻
    uint64_t boost_start_time;         // ブースト開始時刻
    bool boost_active;                 // ブースト中フラグ
    
    void applyPWM(Motor motor, int8_t speed);
};

#endif // MOTOR_DRIVER_H
