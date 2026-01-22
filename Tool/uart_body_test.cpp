#include <iostream>
#include <string>
#include <thread>
#include <atomic>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>

std::atomic<bool> running(true);
int uart_fd = -1;

// UART初期化
bool initUART(const char* device) {
    uart_fd = open(device, O_RDWR | O_NOCTTY);
    if (uart_fd < 0) {
        std::cerr << "Failed to open UART device: " << device << std::endl;
        return false;
    }
    
    struct termios options;
    tcgetattr(uart_fd, &options);
    
    // 115200 baud, 8N1
    cfsetispeed(&options, B115200);
    cfsetospeed(&options, B115200);
    
    options.c_cflag = (options.c_cflag & ~CSIZE) | CS8;
    options.c_cflag &= ~(PARENB | PARODD);
    options.c_cflag &= ~CSTOPB;
    options.c_cflag |= (CLOCAL | CREAD);
    
    options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    options.c_iflag &= ~(IXON | IXOFF | IXANY);
    options.c_oflag &= ~OPOST;
    
    options.c_cc[VMIN] = 0;
    options.c_cc[VTIME] = 1;
    
    tcsetattr(uart_fd, TCSANOW, &options);
    
    std::cout << "UART initialized: " << device << " at 115200 baud" << std::endl;
    return true;
}

// UARTコマンド送信
void sendCommand(char cmd, int8_t value) {
    uint8_t data[2] = {(uint8_t)cmd, (uint8_t)value};
    if (write(uart_fd, data, 2) != 2) {
        std::cerr << "Failed to send command" << std::endl;
    } else {
        std::cout << "Sent: " << cmd << (int)value << std::endl;
    }
}

// IMUデータ受信スレッド
void receiveThread() {
    char buffer[256];
    std::string line_buffer;
    
    while (running) {
        int n = read(uart_fd, buffer, sizeof(buffer) - 1);
        if (n > 0) {
            buffer[n] = '\0';
            line_buffer += buffer;
            
            // 改行で分割
            size_t pos;
            while ((pos = line_buffer.find('\n')) != std::string::npos) {
                std::string line = line_buffer.substr(0, pos);
                line_buffer.erase(0, pos + 1);
                
                if (line.find("IMU,") == 0) {
                    std::cout << "\r\033[K" << line << std::flush;
                } else if (!line.empty()) {
                    std::cout << "\n" << line << std::endl;
                }
            }
        }
        usleep(10000); // 10ms
    }
}

// キーボード入力スレッド
void inputThread() {
    std::cout << "\n=== UART Body Test ===" << std::endl;
    std::cout << "Commands:" << std::endl;
    std::cout << "  r[value]  - Rotation motor (-100 to 100)" << std::endl;
    std::cout << "  d[value]  - Drive motor (-100 to 100)" << std::endl;
    std::cout << "  s         - Stop all motors" << std::endl;
    std::cout << "  q         - Quit" << std::endl;
    std::cout << "\nExample: r50, d-30, s" << std::endl;
    std::cout << ">> " << std::flush;
    
    std::string input;
    while (running && std::getline(std::cin, input)) {
        if (input.empty()) {
            std::cout << ">> " << std::flush;
            continue;
        }
        
        char cmd = input[0];
        
        if (cmd == 'q' || cmd == 'Q') {
            running = false;
            break;
        }
        
        if (cmd == 's' || cmd == 'S') {
            sendCommand('S', 0);
        } else if (cmd == 'r' || cmd == 'R' || cmd == 'd' || cmd == 'D') {
            try {
                int value = std::stoi(input.substr(1));
                if (value < -100) value = -100;
                if (value > 100) value = 100;
                sendCommand(toupper(cmd), (int8_t)value);
            } catch (...) {
                std::cout << "Invalid value. Use: " << cmd << "[number]" << std::endl;
            }
        } else {
            std::cout << "Unknown command: " << cmd << std::endl;
        }
        
        std::cout << ">> " << std::flush;
    }
}

int main(int argc, char* argv[]) {
    const char* uart_device = "/dev/ttyAMA0";
    
    if (argc > 1) {
        uart_device = argv[1];
    }
    
    if (!initUART(uart_device)) {
        return 1;
    }
    
    // 受信スレッド開始
    std::thread recv_thread(receiveThread);
    
    // キーボード入力処理
    inputThread();
    
    // 終了処理
    running = false;
    if (recv_thread.joinable()) {
        recv_thread.join();
    }
    
    // 停止コマンド送信
    sendCommand('S', 0);
    
    close(uart_fd);
    std::cout << "\nTest finished." << std::endl;
    
    return 0;
}
