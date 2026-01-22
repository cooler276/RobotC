#ifndef MPU6886_H
#define MPU6886_H

#include "pico/stdlib.h"
#include "hardware/i2c.h"

class MPU6886 {
public:
    MPU6886();
    bool init();
    bool update();
    
    float getAccelX() const { return accel_x; }
    float getAccelY() const { return accel_y; }
    float getAccelZ() const { return accel_z; }
    float getGyroX() const { return gyro_x; }
    float getGyroY() const { return gyro_y; }
    float getGyroZ() const { return gyro_z; }
    
    // キャリブレーション用：座標軸変換設定
    // 例: setAxisMapping(0, 1, 2, false, false, false) = そのまま
    //     setAxisMapping(1, 0, 2, true, false, false) = X↔Y入れ替え、X反転
    void setAxisMapping(int x_axis, int y_axis, int z_axis, 
                        bool x_invert, bool y_invert, bool z_invert);
    
private:
    static constexpr uint8_t MPU6886_ADDRESS = 0x68;
    static constexpr uint8_t WHO_AM_I = 0x75;
    static constexpr uint8_t PWR_MGMT_1 = 0x6B;
    static constexpr uint8_t ACCEL_CONFIG = 0x1C;
    static constexpr uint8_t GYRO_CONFIG = 0x1B;
    static constexpr uint8_t ACCEL_XOUT_H = 0x3B;
    
    // 実際の配線に従ったピン配置
    static constexpr uint I2C_SDA_PIN = 26;  // GPIO26 (I2C1_SDA)
    static constexpr uint I2C_SCL_PIN = 27;  // GPIO27 (I2C1_SCL)
    static constexpr uint I2C_BAUDRATE = 400000;
    
    float accel_x, accel_y, accel_z;
    float gyro_x, gyro_y, gyro_z;
    
    // 座標軸変換設定（デフォルト：変換なし）
    int axis_map[3] = {0, 1, 2};      // X,Y,Z → どの軸を使うか
    bool axis_invert[3] = {false, false, false};  // 各軸を反転するか
    
    bool writeRegister(uint8_t reg, uint8_t data);
    bool readRegisters(uint8_t reg, uint8_t* buffer, size_t len);
};

#endif // MPU6886_H
