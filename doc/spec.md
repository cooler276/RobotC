# Specification

This document contains the specification for the RobotC project.

## Overview

RobotC is a robot project that consists of:
- PC for robot control and TTS
- Raspberry Pi Zero 2W for camera, depth sensor, and audio
- Raspberry Pi Pico for IMU and motor control

## Communication

- PC ↔ Raspberry Pi Zero 2W: MQTT
- Raspberry Pi Zero 2W ↔ Raspberry Pi Pico: UART
