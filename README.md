# RobotC
my robot project

```mermaid
graph TD
    PC["PC (Robot Control / TTS)"]

    subgraph Robot
        Zero2W["Raspberry Pi Zero 2W<br>(Camera / Depth / Audio)"]
        Pico["Raspberry Pi Pico<br>(IMU / Motor Control)"]

        Camera["Camera"] --- Zero2W
        Depth["Depth Sensor"] --- Zero2W
        Mic["Microphone"] --- Zero2W
        Speaker["Speaker"] --- Zero2W

        IMU["IMU"] --- Pico
        Motor["Motor Driver"] --- Pico
    end

    PC <-- "MQTT" --> Zero2W
    Zero2W <-- "UART" --> Pico

    style PC fill:#f9f,stroke:#333
    style Zero2W fill:#bbf,stroke:#333
    style Pico fill:#dfd,stroke:#333
```
