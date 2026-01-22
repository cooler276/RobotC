#include "i2c_dev.h"

#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <cstring>
#include <iostream>

I2CDev::I2CDev(const std::string& device, uint8_t addr)
  : device_(device), fd_(-1), addr_(addr) {}

I2CDev::~I2CDev() { closeDevice(); }

bool I2CDev::openDevice() {
  fd_ = ::open(device_.c_str(), O_RDWR);
  if (fd_ < 0) return false;
  if (ioctl(fd_, I2C_SLAVE, addr_) < 0) {
    ::close(fd_);
    fd_ = -1;
    return false;
  }
  return true;
}

void I2CDev::closeDevice() {
  if (fd_ >= 0) {
    ::close(fd_);
    fd_ = -1;
  }
}

ssize_t I2CDev::writeRegister(uint16_t reg, const uint8_t* data, size_t len) {
  if (fd_ < 0) return -1;
  // register high byte, low byte, then data
  size_t buf_len = 2 + len;
  uint8_t* buf = new uint8_t[buf_len];
  buf[0] = static_cast<uint8_t>((reg >> 8) & 0xFF);
  buf[1] = static_cast<uint8_t>(reg & 0xFF);
  if (len > 0) memcpy(buf + 2, data, len);
  ssize_t w = ::write(fd_, buf, buf_len);
  delete [] buf;
  return w;
}

ssize_t I2CDev::readRegister(uint16_t reg, uint8_t* buf, size_t len) {
  if (fd_ < 0) return -1;
  uint8_t addr_buf[2];
  addr_buf[0] = static_cast<uint8_t>((reg >> 8) & 0xFF);
  addr_buf[1] = static_cast<uint8_t>(reg & 0xFF);
  // write register address
  if (::write(fd_, addr_buf, 2) != 2) return -1;
  // read data
  ssize_t r = ::read(fd_, buf, len);
  return r;
}
