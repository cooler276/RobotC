#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <cstring>
#include <cstdint>
#include <chrono>
#include <thread>
#include <string>

#include "platform.h"
#include "vl53l8cx_api.h"

static int g_fd = -1;
static std::string g_device = "/dev/i2c-1";
static uint16_t g_addr7 = 0x29; // 7-bit default (0x52 >> 1)

// Optional initializer used by main before calling API functions
extern "C" void platform_init_i2c(const char* device, uint16_t address_8bit)
{
    if(g_fd >= 0) {
        ::close(g_fd);
        g_fd = -1;
    }
    g_device = device ? device : "/dev/i2c-1";
    // ST API uses 8-bit address (0x52). Convert to 7-bit for ioctl.
    g_addr7 = (uint16_t)(address_8bit >> 1);
    g_fd = ::open(g_device.c_str(), O_RDWR);
    if (g_fd >= 0) {
        ioctl(g_fd, I2C_SLAVE, g_addr7);
    }
}

extern "C" uint8_t VL53L8CX_RdByte(
    VL53L8CX_Platform *p_platform,
    uint16_t RegisterAdress,
    uint8_t *p_value)
{
    if (g_fd < 0) { platform_init_i2c("/dev/i2c-1", p_platform->address); }
    uint8_t buf[2];
    buf[0] = (uint8_t)(RegisterAdress >> 8);
    buf[1] = (uint8_t)(RegisterAdress & 0xFF);
    if (::write(g_fd, buf, 2) != 2) return (uint8_t)VL53L8CX_STATUS_ERROR;
    if (::read(g_fd, p_value, 1) != 1) return (uint8_t)VL53L8CX_STATUS_ERROR;
    return (uint8_t)VL53L8CX_STATUS_OK;
}

extern "C" uint8_t VL53L8CX_WrByte(
    VL53L8CX_Platform *p_platform,
    uint16_t RegisterAdress,
    uint8_t value)
{
    if (g_fd < 0) { platform_init_i2c("/dev/i2c-1", p_platform->address); }
    uint8_t buf[3];
    buf[0] = (uint8_t)(RegisterAdress >> 8);
    buf[1] = (uint8_t)(RegisterAdress & 0xFF);
    buf[2] = value;
    if (::write(g_fd, buf, 3) != 3) return (uint8_t)VL53L8CX_STATUS_ERROR;
    return (uint8_t)VL53L8CX_STATUS_OK;
}

extern "C" uint8_t VL53L8CX_RdMulti(
    VL53L8CX_Platform *p_platform,
    uint16_t RegisterAdress,
    uint8_t *p_values,
    uint32_t size)
{
    if (g_fd < 0) { platform_init_i2c("/dev/i2c-1", p_platform->address); }
    const uint32_t MAX_XFER = 64;
    uint32_t remaining = size;
    uint32_t offset = 0;
    while (remaining > 0) {
        uint32_t chunk = remaining > MAX_XFER ? MAX_XFER : remaining;
        uint16_t reg = (uint16_t)(RegisterAdress + offset);
        uint8_t addr_buf[2];
        addr_buf[0] = (uint8_t)(reg >> 8);
        addr_buf[1] = (uint8_t)(reg & 0xFF);
        if (::write(g_fd, addr_buf, 2) != 2) return (uint8_t)VL53L8CX_STATUS_ERROR;
        ssize_t r = ::read(g_fd, p_values + offset, chunk);
        if ((uint32_t)r != chunk) return (uint8_t)VL53L8CX_STATUS_ERROR;
        offset += chunk;
        remaining -= chunk;
    }
    return (uint8_t)VL53L8CX_STATUS_OK;
}

extern "C" uint8_t VL53L8CX_WrMulti(
    VL53L8CX_Platform *p_platform,
    uint16_t RegisterAdress,
    uint8_t *p_values,
    uint32_t size)
{
    if (g_fd < 0) { platform_init_i2c("/dev/i2c-1", p_platform->address); }
    const uint32_t MAX_XFER = 64;
    uint32_t remaining = size;
    uint32_t offset = 0;
    while (remaining > 0) {
        uint32_t chunk = remaining > MAX_XFER ? MAX_XFER : remaining;
        uint16_t reg = (uint16_t)(RegisterAdress + offset);
        uint32_t total = 2 + chunk;
        uint8_t* buf = new uint8_t[total];
        buf[0] = (uint8_t)(reg >> 8);
        buf[1] = (uint8_t)(reg & 0xFF);
        memcpy(buf + 2, p_values + offset, chunk);
        ssize_t w = ::write(g_fd, buf, total);
        delete [] buf;
        if ((uint32_t)w != total) return (uint8_t)VL53L8CX_STATUS_ERROR;
        offset += chunk;
        remaining -= chunk;
    }
    return (uint8_t)VL53L8CX_STATUS_OK;
}

extern "C" uint8_t VL53L8CX_Reset_Sensor(VL53L8CX_Platform *p_platform)
{
    // No hardware reset implemented in this wrapper
    (void)p_platform;
    return (uint8_t)VL53L8CX_STATUS_OK;
}

extern "C" void VL53L8CX_SwapBuffer(uint8_t *buffer, uint16_t size)
{
    // Swap every 4 bytes (endian conversion used by API)
    for (uint16_t i = 0; i + 3 < size; i += 4) {
        uint8_t b0 = buffer[i];
        buffer[i] = buffer[i+3];
        buffer[i+3] = b0;
        uint8_t b1 = buffer[i+1];
        buffer[i+1] = buffer[i+2];
        buffer[i+2] = b1;
    }
}

extern "C" uint8_t VL53L8CX_WaitMs(VL53L8CX_Platform *p_platform, uint32_t TimeMs)
{
    (void)p_platform;
    std::this_thread::sleep_for(std::chrono::milliseconds(TimeMs));
    return (uint8_t)VL53L8CX_STATUS_OK;
}
