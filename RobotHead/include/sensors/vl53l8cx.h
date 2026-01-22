#pragma once

#include "i2c_dev.h"
#include <cstdint>

class VL53L8CX {
public:
  explicit VL53L8CX(I2CDev& bus);
  bool init();
  bool startRanging();
  bool readRange(uint16_t& distance_mm);
private:
  I2CDev& bus_;
};
