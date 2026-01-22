CXX = aarch64-linux-gnu-g++
CC = aarch64-linux-gnu-gcc
CXXFLAGS = -std=c++17 -I/home/ryo/work/RobotC/includes -I/usr/include/opencv4 -I../include
LDFLAGS = -L/home/ryo/work/RobotC/libs/aarch64-linux-gnu -lopencv_core -lopencv_highgui -lopencv_imgproc -lopencv_videoio -lopencv_imgcodecs

SOURCES = src/main.cpp src/vl53l8cx.cpp src/platform_wrapper.cpp src/i2c_dev.cpp
OBJECTS = $(SOURCES:.cpp=.o)
TARGET = robot_head

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)