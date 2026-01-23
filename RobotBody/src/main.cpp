#include <stdio.h>
#include <cstdlib>
#include <cmath>
#include "pico/stdlib.h"
#include "mpu6886.h"
#include "motor_driver.h"
#include "uart_handler.h"

// バージョン情報
#define ROBOT_BODY_VERSION "1.0.0"

// 異常検知閾値
#define FALL_THRESHOLD_DEG 45.0f  // 転倒検知：ロール/ピッチ45度以上
#define LIFT_THRESHOLD_G 0.5f     // 持ち上げ検知：Z軸加速度0.5G未満
#define COMM_TIMEOUT_MS 5000      // 通信タイムアウト：5秒

// グローバルオブジェクト
MPU6886 imu;
MotorDriver motor;
UARTHandler uart;

// 状態管理
bool imu_available = false;
uint64_t last_command_time = 0;
bool reduced_mode = false;

// UART送信ヘルパー
void sendUARTMessage(const char* msg) {
    uart.send((uint8_t*)msg, strlen(msg));
}

// コマンド処理（ASCII文字列対応）
void handleCommand(const uint8_t* data, size_t len) {
    if (len < 1) return;
    
    // 通信タイムアウト更新
    last_command_time = to_ms_since_boot(get_absolute_time());
    if (reduced_mode) {
        reduced_mode = false;
        printf("Exiting reduced mode\n");
    }
    
    char cmd = data[0];
    int value = 0;
    
    // 数値部分を解析（例: "R50" → 50, "D-30" → -30）
    if (len > 1) {
        value = atoi((const char*)(data + 1));
    }
    
    // 文字列コマンドの判定（CALIB, RESETなど）
    if (len >= 5 && strncmp((const char*)data, "CALIB", 5) == 0) {
        printf("Calibration command received\n");
        if (imu_available) {
            // キャリブレーション処理（現状は既設定値を使用）
            sendUARTMessage("ACK,CALIB\n");
            printf("Calibration acknowledged\n");
        } else {
            sendUARTMessage("NG,CALIB,IMU_NOT_AVAILABLE\n");
        }
        return;
    }
    
    if (len >= 5 && strncmp((const char*)data, "RESET", 5) == 0) {
        printf("Reset command received\n");
        motor.stop();
        sendUARTMessage("ACK,RESET\n");
        printf("System reset acknowledged\n");
        return;
    }
    
    printf("CMD: %c, Value: %d\n", cmd, value);
    
    switch (cmd) {
        case 'R':
        case 'r': // 回転モーター
            motor.setSpeed(MotorDriver::MOTOR_ROTATION, value);
            printf("Rotation motor: %d\n", value);
            break;
            
        case 'D':
        case 'd': // 前進後退モーター
            motor.setSpeed(MotorDriver::MOTOR_DRIVE, value);
            printf("Drive motor: %d\n", value);
            break;
            
        case 'S':
        case 's': // 停止
            motor.stop();
            printf("Motors stopped\n");
            break;
            
        default:
            printf("Unknown command: %c\n", cmd);
            break;
    }
}

// IMUデータ送信
void sendIMUData() {
    char buffer[64];
    int len = snprintf(buffer, sizeof(buffer), 
        "IMU,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f\n",
        imu.getAccelX(), imu.getAccelY(), imu.getAccelZ(),
        imu.getGyroX(), imu.getGyroY(), imu.getGyroZ()
    );
    
    if (len > 0) {
        uart.send((uint8_t*)buffer, len);
    }
}

// 異常運動検知
void checkAbnormalMotion() {
    if (!imu_available) return;
    
    float ax = imu.getAccelX();
    float ay = imu.getAccelY();
    float az = imu.getAccelZ();
    
    // ロール・ピッチ角度計算（簡易版）
    float roll = atan2(ay, az) * 180.0f / M_PI;
    float pitch = atan2(-ax, sqrt(ay * ay + az * az)) * 180.0f / M_PI;
    
    // 転倒検知（ロール/ピッチが閾値超過）
    if (fabs(roll) > FALL_THRESHOLD_DEG || fabs(pitch) > FALL_THRESHOLD_DEG) {
        motor.stop();
        sendUARTMessage("ALERT,FALL\n");
        printf("FALL DETECTED! Roll: %.1f, Pitch: %.1f\n", roll, pitch);
    }
    
    // 持ち上げ検知（Z軸加速度が低下）
    float accel_magnitude = sqrt(ax * ax + ay * ay + az * az);
    if (accel_magnitude < LIFT_THRESHOLD_G) {
        motor.stop();
        sendUARTMessage("ALERT,LIFT\n");
        printf("LIFT DETECTED! Accel: %.2f\n", accel_magnitude);
    }
}

int main() {
    // 初期化
    stdio_init_all();
    sleep_ms(2000);  // UART安定化を待つ
    
    printf("Robot Body Starting...\n");
    
    // UART初期化
    if (!uart.init()) {
        printf("UART initialization failed\n");
        return -1;
    }
    uart.setCommandCallback(handleCommand);
    printf("UART initialized\n");
    
    // IMU初期化（失敗しても続行）
    imu_available = imu.init();
    if (imu_available) {
        printf("IMU initialized\n");
        
        // IMU座標軸キャリブレーション結果
        // flat:   X= 0.34, Y= 9.77, Z=-0.44
        // front:  X=-1.38, Y= 9.15, Z= 4.12
        // back:   X= 0.50, Y= 7.11, Z=-6.03
        // left:   X= 5.79, Y= 7.39, Z= 3.56
        // right:  X=-5.29, Y= 8.30, Z= 0.01
        // upside: X= 0.41, Y=-9.55, Z= 2.23
        imu.setAxisMapping(2, 0, 1, false, false, false);
    } else {
        printf("IMU not available - continuing without IMU\n");
    }
    
    // モータードライバ初期化
    if (!motor.init()) {
        printf("Motor driver initialization failed\n");
        return -1;
    }
    printf("Motor driver initialized\n");
    
    // 起動通知（バージョン情報）
    char info_msg[32];
    snprintf(info_msg, sizeof(info_msg), "INFO,%s\n", ROBOT_BODY_VERSION);
    sendUARTMessage(info_msg);
    printf("Robot Body Ready! Version: %s\n", ROBOT_BODY_VERSION);
    
    // メインループ
    uint64_t last_imu_time = 0;
    const uint64_t IMU_INTERVAL_MS = 50; // 20Hz
    last_command_time = to_ms_since_boot(get_absolute_time());
    
    while (true) {
        uint64_t now = to_ms_since_boot(get_absolute_time());
        
        // UART受信処理
        uart.update();
        
        // モーター制御更新（ランプ・ブースト制御）
        motor.update();
        
        // 通信タイムアウト検知（縮小運転モード）
        if (!reduced_mode && (now - last_command_time > COMM_TIMEOUT_MS)) {
            reduced_mode = true;
            motor.stop();
            printf("Communication timeout - entering reduced mode\n");
        }
        
        // IMUデータ更新・送信（IMU有効時のみ）
        if (imu_available) {
            if (now - last_imu_time >= IMU_INTERVAL_MS) {
                if (imu.update()) {
                    sendIMUData();
                    checkAbnormalMotion();
                }
                last_imu_time = now;
            }
        }
        
        sleep_ms(1);
    }
    
    return 0;
}
