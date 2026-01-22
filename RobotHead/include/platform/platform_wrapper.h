#pragma once

#include <cstdint>

extern "C" {
  void platform_init_i2c(const char* device, uint16_t address_8bit);
}
