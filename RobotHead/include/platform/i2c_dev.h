#pragma once

#include <string>
#include <cstdint>

class I2CDev {
public:
  I2CDev(const std::string& device, uint8_t addr);
  ~I2CDev();
  bool openDevice();
  void closeDevice();
  ssize_t writeRegister(uint16_t reg, const uint8_t* data, size_t len);
  ssize_t readRegister(uint16_t reg, uint8_t* buf, size_t len);
private:
  std::string device_;
  int fd_;
  uint8_t addr_;
};
