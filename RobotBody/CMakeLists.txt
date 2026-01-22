cmake_minimum_required(VERSION 3.13)

# Pico SDKを初期化
set(PICO_SDK_PATH "${CMAKE_SOURCE_DIR}/../pico-sdk")
include(${PICO_SDK_PATH}/pico_sdk_init.cmake)

project(RobotBody C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Pico 2 (RP2350) を指定
set(PICO_BOARD pico2)

pico_sdk_init()

# インクルードディレクトリ
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/imu
    ${CMAKE_SOURCE_DIR}/include/motor
    ${CMAKE_SOURCE_DIR}/include/communication
)

# 実行ファイル（テスト用）
add_executable(test_uart
    src/test_uart.cpp
)

# Pico SDKライブラリをリンク
target_link_libraries(test_uart
    pico_stdlib
)

# UART経由のstdio出力を有効化
pico_enable_stdio_usb(test_uart 0)
pico_enable_stdio_uart(test_uart 1)

# UF2ファイル生成
pico_add_extra_outputs(test_uart)

# 実行ファイル（メイン）
add_executable(robot_body
    src/main.cpp
    src/imu/mpu6886.cpp
    src/motor/motor_driver.cpp
    src/communication/uart_handler.cpp
)

# Pico SDKライブラリをリンク
target_link_libraries(robot_body
    pico_stdlib
    hardware_i2c
    hardware_pwm
    hardware_uart
)

# UART経由のstdio出力を有効化（デバッグ用）
pico_enable_stdio_usb(robot_body 0)
pico_enable_stdio_uart(robot_body 1)

# UF2ファイル生成
pico_add_extra_outputs(robot_body)
