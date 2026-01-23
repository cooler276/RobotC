cmake_minimum_required(VERSION 3.10)

# プロジェクト名とC++の標準を設定
project(RobotHead LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# 物体検出機能のオプション（デフォルトはON）
option(ENABLE_OBJECT_DETECTION "Enable object detection feature" ON)

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

# ソースファイルを指定
set(SOURCES
    src/main.cpp
)

# インクルードディレクトリを指定
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/sensors
    ${CMAKE_SOURCE_DIR}/include/detection
    ${CMAKE_SOURCE_DIR}/include/platform
    ${CMAKE_SOURCE_DIR}/include/camera
    ${CMAKE_SOURCE_DIR}/include/network
    ${CMAKE_SOURCE_DIR}/include/audio
    ${CMAKE_SOURCE_DIR}/include/hardware
    ${CMAKE_SOURCE_DIR}/../include
    ${CMAKE_SOURCE_DIR}/../includes
    ${CMAKE_SOURCE_DIR}/../opencv_4.10_headers
    ${CMAKE_SOURCE_DIR}/../libs/aarch64/aarch64-linux-gnu/usr/include/libcamera
)

# ライブラリのパスを指定
link_directories(${CMAKE_SOURCE_DIR}/../libs/aarch64/aarch64-linux-gnu)

# VL53L8CXライブラリのソースファイルを追加
set(VL53L8CX_SOURCES
    src/sensors/vl53l8cx.cpp
    src/platform/platform_wrapper.cpp
    src/platform/i2c_dev.cpp
)

# VL53L8CXライブラリをターゲットにリンク
set(API_SRC
  src/sensors/vl53l8cx_api.c
  src/sensors/vl53l8cx_plugin_motion_indicator.c
  src/sensors/vl53l8cx_plugin_detection_thresholds.c
  src/sensors/vl53l8cx_plugin_xtalk.c
)

# Ensure ST C sources are compiled as C (not C++)
set_source_files_properties(${API_SRC} PROPERTIES LANGUAGE C)

# Enable C language explicitly
enable_language(C)

# Add API_SRC to vl53l8cx_lib
add_library(vl53l8cx_lib ${VL53L8CX_SOURCES} ${API_SRC})

# ソースファイルのリスト
set(ROBOT_HEAD_SOURCES
  src/main.cpp
  src/platform/i2c_dev.cpp
  src/sensors/vl53l8cx.cpp
  src/platform/platform_wrapper.cpp
  src/hardware/uart_pico.cpp
  src/audio/audio_player.cpp
  src/audio/voice_detector.cpp
  src/hardware/led_controller.cpp
  src/camera/libcamera_capture.cpp
  ${API_SRC}
)

# 物体検出機能が有効な場合、object_detector.cppを追加
if(ENABLE_OBJECT_DETECTION)
  list(APPEND ROBOT_HEAD_SOURCES src/detection/object_detector.cpp)
  add_definitions(-DENABLE_OBJECT_DETECTION)
endif()

# 実行ファイルを作成
add_executable(robot_head ${ROBOT_HEAD_SOURCES})

# Explicitly specify the path to the static library
set(VL53L8CX_LIB_PATH ${CMAKE_BINARY_DIR}/libvl53l8cx_lib.a)

# OpenCVライブラリを手動でリンク
set(OPENCV_LIBS
    /home/ryo/work/RobotC/libs/aarch64/libopencv_dnn.so
    /home/ryo/work/RobotC/libs/aarch64/libopencv_calib3d.so
    /home/ryo/work/RobotC/libs/aarch64/libopencv_highgui.so
    /home/ryo/work/RobotC/libs/aarch64/libopencv_videoio.so
    /home/ryo/work/RobotC/libs/aarch64/libopencv_imgcodecs.so
    /home/ryo/work/RobotC/libs/aarch64/libopencv_imgproc.so
    /home/ryo/work/RobotC/libs/aarch64/libopencv_core.so
)

# Add paths for LAPACK and BLAS libraries
set(LAPACK_LIBS
    /home/ryo/work/RobotC/libs/aarch64/lapack/liblapack.so.3
    /home/ryo/work/RobotC/libs/aarch64/blas/libblas.so.3
)

# Add path for Armadillo library
set(ARMADILLO_LIBS
    /home/ryo/work/RobotC/libs/aarch64/libarmadillo.so.14
)

# Add path for ALSA library
set(ALSA_LIBS
    /home/ryo/work/RobotC/libs/aarch64/libasound.so.2
)

# Add path for libcamera libraries
set(LIBCAMERA_LIBS
    /home/ryo/work/RobotC/libs/aarch64/libcamera.so
    /home/ryo/work/RobotC/libs/aarch64/libcamera-base.so
)

# VL53L8CXライブラリとスレッドライブラリをリンク
target_link_libraries(robot_head pthread ${ALSA_LIBS} ${OPENCV_LIBS} ${LAPACK_LIBS} ${ARMADILLO_LIBS} ${LIBCAMERA_LIBS} vl53l8cx_lib)

# Add dependency to ensure proper linking order
add_dependencies(robot_head vl53l8cx_lib)