#include "uart_handler.h"

UARTHandler::UARTHandler() 
    : command_callback(nullptr), rx_index(0) {}

bool UARTHandler::init() {
    // UART0初期化 (GPIO0=TX, GPIO1=RX)
    uart_init(uart0, UART_BAUD);
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);
    
    // FIFO有効化
    uart_set_fifo_enabled(uart0, true);
    
    return true;
}

void UARTHandler::update() {
    // 受信データ処理（改行区切りASCIIコマンド対応）
    while (uart_is_readable(uart0)) {
        uint8_t byte = uart_getc(uart0);
        
        if (byte == '\n' || byte == '\r') {
            // 改行でコマンド確定
            if (rx_index > 0 && command_callback) {
                rx_buffer[rx_index] = '\0';  // NULL終端
                command_callback(rx_buffer, rx_index);
                rx_index = 0;
            }
        } else if (rx_index < BUFFER_SIZE - 1) {
            rx_buffer[rx_index++] = byte;
        } else {
            // バッファオーバーフロー
            rx_index = 0;
        }
    }
}

void UARTHandler::send(const uint8_t* data, size_t len) {
    uart_write_blocking(uart0, data, len);
}

void UARTHandler::setCommandCallback(CommandCallback callback) {
    command_callback = callback;
}
