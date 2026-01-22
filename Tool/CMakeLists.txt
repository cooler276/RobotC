cmake_minimum_required(VERSION 3.10)

# AArch64用のクロスコンパイルツールチェーンを指定
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)
set(CMAKE_C_COMPILER aarch64-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)
set(CMAKE_LINKER aarch64-linux-gnu-ld)
set(CMAKE_FIND_ROOT_PATH /home/ryo/work/RobotC/libs/aarch64/aarch64-linux-gnu)

# CMakeのライブラリ検索パスを制限
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

project(RobotC_Tools LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)

# インクルードディレクトリを指定
include_directories(
    /usr/include/opencv4
    ${CMAKE_SOURCE_DIR}/../include
    ${CMAKE_SOURCE_DIR}/../includes
)

# ライブラリのパスを指定
link_directories(${CMAKE_SOURCE_DIR}/../libs/aarch64/aarch64-linux-gnu)

# OpenCVライブラリを手動で指定
set(OPENCV_LIBS
    /home/ryo/work/RobotC/libs/aarch64/libopencv_core.so
    /home/ryo/work/RobotC/libs/aarch64/libopencv_highgui.so
    /home/ryo/work/RobotC/libs/aarch64/libopencv_imgproc.so
    /home/ryo/work/RobotC/libs/aarch64/libopencv_videoio.so
    /home/ryo/work/RobotC/libs/aarch64/libopencv_imgcodecs.so
    /home/ryo/work/RobotC/libs/aarch64/libopencv_calib3d.so
)

# LAPACK/BLASライブラリを指定
set(LAPACK_LIBS
    /home/ryo/work/RobotC/libs/aarch64/lapack/liblapack.so.3
    /home/ryo/work/RobotC/libs/aarch64/blas/libblas.so.3
)

# Armadilloライブラリを指定
set(ARMADILLO_LIBS
    /home/ryo/work/RobotC/libs/aarch64/libarmadillo.so.14
)

# VL53L8CXライブラリのソースファイル
set(VL53L8CX_SOURCES
    ../RobotHead/src/sensors/vl53l8cx.cpp
    ../RobotHead/src/platform/platform_wrapper.cpp
    ../RobotHead/src/platform/i2c_dev.cpp
)

set(API_SRC
    ../RobotHead/src/sensors/vl53l8cx_api.c
    ../RobotHead/src/sensors/vl53l8cx_plugin_motion_indicator.c
    ../RobotHead/src/sensors/vl53l8cx_plugin_detection_thresholds.c
    ../RobotHead/src/sensors/vl53l8cx_plugin_xtalk.c
)

set_source_files_properties(${API_SRC} PROPERTIES LANGUAGE C)
enable_language(C)

# カメラキャリブレーションツール(HTTPベース)
add_executable(camera_calibration_web camera_calibration_web.cpp)
target_link_libraries(camera_calibration_web ${OPENCV_LIBS} ${LAPACK_LIBS} ${ARMADILLO_LIBS} pthread)

# Depthキャリブレーションツール(HTTPベース)
add_executable(depth_calibration_web depth_calibration_web.cpp ${VL53L8CX_SOURCES} ${API_SRC})
target_include_directories(depth_calibration_web PRIVATE 
    ${CMAKE_SOURCE_DIR}/../RobotHead/include
    ${CMAKE_SOURCE_DIR}/../RobotHead/include/sensors
    ${CMAKE_SOURCE_DIR}/../RobotHead/include/platform
    ${CMAKE_SOURCE_DIR}/../include
    ${CMAKE_SOURCE_DIR}/../includes
)
target_link_libraries(depth_calibration_web ${OPENCV_LIBS} ${LAPACK_LIBS} ${ARMADILLO_LIBS} pthread)

# Depthキャリブレーションツール(旧版)
add_executable(calibrate_depth calibrate_depth.cpp)
target_link_libraries(calibrate_depth ${OPENCV_LIBS} ${LAPACK_LIBS} ${ARMADILLO_LIBS})
# UARTボディテストツール
add_executable(uart_body_test uart_body_test.cpp)
target_link_libraries(uart_body_test pthread)