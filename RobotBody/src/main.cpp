#include <stdio.h>
#include <cstdlib>
#include "pico/stdlib.h"
#include "mpu6886.h"
#include "motor_driver.h"
#include "uart_handler.h"

// グローバルオブジェクト
MPU6886 imu;
MotorDriver motor;
UARTHandler uart;

// コマンド処理（ASCII文字列対応）
void handleCommand(const uint8_t* data, size_t len) {
    if (len < 1) return;
    
    char cmd = data[0];
    int value = 0;
    
    // 数値部分を解析（例: "R50" → 50, "D-30" → -30）
    if (len > 1) {
        value = atoi((const char*)(data + 1));
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
    bool imu_available = imu.init();
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
    
    printf("Robot Body Ready!\n");
    
    // メインループ
    uint64_t last_imu_time = 0;
    const uint64_t IMU_INTERVAL_MS = 50; // 20Hz
    
    while (true) {
        // UART受信処理
        uart.update();
        
        // IMUデータ更新・送信（IMU有効時のみ）
        if (imu_available) {
            uint64_t now = to_ms_since_boot(get_absolute_time());
            if (now - last_imu_time >= IMU_INTERVAL_MS) {
                if (imu.update()) {
                    sendIMUData();
                }
                last_imu_time = now;
            }
        }
        
        sleep_ms(1);
    }
    
    return 0;
}
