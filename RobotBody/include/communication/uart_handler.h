#ifndef UART_HANDLER_H
#define UART_HANDLER_H

#include "pico/stdlib.h"
#include "pico/types.h"
#include "hardware/uart.h"

typedef void (*CommandCallback)(const uint8_t* data, size_t len);

class UARTHandler {
public:
    UARTHandler();
    bool init();
    void update();
    void send(const uint8_t* data, size_t len);
    void setCommandCallback(CommandCallback callback);
    
private:
    // spec.mdに従ったピン配置
    static constexpr uint UART_TX_PIN = 0;  // GPIO0
    static constexpr uint UART_RX_PIN = 1;  // GPIO1
    static constexpr uint UART_BAUD = 115200;
    static constexpr size_t BUFFER_SIZE = 256;
    
    CommandCallback command_callback;
    uint8_t rx_buffer[BUFFER_SIZE];
    size_t rx_index;
};

#endif // UART_HANDLER_H
