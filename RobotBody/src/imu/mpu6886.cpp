#include "mpu6886.h"
#include <cstdio>
#include <cmath>

MPU6886::MPU6886() 
    : accel_x(0), accel_y(0), accel_z(0),
      gyro_x(0), gyro_y(0), gyro_z(0) {}

bool MPU6886::init() {
    printf("IMU: Initializing I2C1...\n");
    // I2C1初期化 (GPIO27=SCL, GPIO26=SDA)
    i2c_init(i2c1, I2C_BAUDRATE);
    gpio_set_function(I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA_PIN);
    gpio_pull_up(I2C_SCL_PIN);
    
    sleep_ms(100);
    
    printf("IMU: Reading WHO_AM_I register...\n");
    // WHO_AM_I確認
    uint8_t who_am_i;
    if (!readRegisters(WHO_AM_I, &who_am_i, 1)) {
        printf("IMU: Failed to read WHO_AM_I\n");
        return false;
    }
    
    printf("IMU: WHO_AM_I = 0x%02X (expected 0x19)\n", who_am_i);
    if (who_am_i != 0x19) {
        printf("IMU: Wrong WHO_AM_I value\n");
        return false;
    }
    
    printf("IMU: Resetting device...\n");
    // リセット
    writeRegister(PWR_MGMT_1, 0x80);
    sleep_ms(100);
    
    printf("IMU: Configuring registers...\n");
    // クロックソース設定
    writeRegister(PWR_MGMT_1, 0x01);
    sleep_ms(10);
    
    // 加速度センサ設定: ±8g
    writeRegister(ACCEL_CONFIG, 0x10);
    
    // ジャイロ設定: ±2000dps
    writeRegister(GYRO_CONFIG, 0x18);
    
    sleep_ms(50);
    
    printf("IMU: Initialization complete\n");
    return true;
}

bool MPU6886::update() {
    uint8_t data[14];
    
    if (!readRegisters(ACCEL_XOUT_H, data, 14)) {
        return false;
    }
    
    // 加速度データ（生データ）
    int16_t ax = (data[0] << 8) | data[1];
    int16_t ay = (data[2] << 8) | data[3];
    int16_t az = (data[4] << 8) | data[5];
    
    // ジャイロデータ（生データ）
    int16_t gx = (data[8] << 8) | data[9];
    int16_t gy = (data[10] << 8) | data[11];
    int16_t gz = (data[12] << 8) | data[13];
    
    // ±8g → m/s²（生データ配列）
    float accel_raw[3] = {
        (ax / 4096.0f) * 9.80665f,
        (ay / 4096.0f) * 9.80665f,
        (az / 4096.0f) * 9.80665f
    };
    
    // ±2000dps → rad/s（生データ配列）
    float gyro_raw[3] = {
        (gx / 16.4f) * (M_PI / 180.0f),
        (gy / 16.4f) * (M_PI / 180.0f),
        (gz / 16.4f) * (M_PI / 180.0f)
    };
    
    // 座標軸変換適用
    accel_x = accel_raw[axis_map[0]] * (axis_invert[0] ? -1.0f : 1.0f);
    accel_y = accel_raw[axis_map[1]] * (axis_invert[1] ? -1.0f : 1.0f);
    accel_z = accel_raw[axis_map[2]] * (axis_invert[2] ? -1.0f : 1.0f);
    
    gyro_x = gyro_raw[axis_map[0]] * (axis_invert[0] ? -1.0f : 1.0f);
    gyro_y = gyro_raw[axis_map[1]] * (axis_invert[1] ? -1.0f : 1.0f);
    gyro_z = gyro_raw[axis_map[2]] * (axis_invert[2] ? -1.0f : 1.0f);
    
    return true;
}

void MPU6886::setAxisMapping(int x_axis, int y_axis, int z_axis,
                              bool x_invert, bool y_invert, bool z_invert) {
    axis_map[0] = x_axis;
    axis_map[1] = y_axis;
    axis_map[2] = z_axis;
    axis_invert[0] = x_invert;
    axis_invert[1] = y_invert;
    axis_invert[2] = z_invert;
    
    printf("IMU: Axis mapping set to [%d,%d,%d] invert=[%d,%d,%d]\n",
           x_axis, y_axis, z_axis, x_invert, y_invert, z_invert);
}

bool MPU6886::writeRegister(uint8_t reg, uint8_t data) {
    uint8_t buf[2] = {reg, data};
    // 100msタイムアウト
    int result = i2c_write_timeout_us(i2c1, MPU6886_ADDRESS, buf, 2, false, 100000);
    return result == 2;
}

bool MPU6886::readRegisters(uint8_t reg, uint8_t* buffer, size_t len) {
    // タイムアウト付きI2C通信 (100ms)
    int result = i2c_write_timeout_us(i2c1, MPU6886_ADDRESS, &reg, 1, true, 100000);
    if (result != 1) {
        printf("IMU: I2C write failed (result=%d)\n", result);
        return false;
    }
    result = i2c_read_timeout_us(i2c1, MPU6886_ADDRESS, buffer, len, false, 100000);
    if (result != (int)len) {
        printf("IMU: I2C read failed (result=%d, expected=%d)\n", result, len);
        return false;
    }
    return true;
}
