## Zero2W ⇔ RP2350 UART Message Protocol

### Zero -> RP2350

Size: 12 Bytes 

| Byte | Field | Type | Unit | Description |
|:---:|:---|:---:|:---:|:---|
| 0 | Header | uint8 | - | 0x5A | 
| 1 | Mode | uint8 | - | 0:STOP, 1:VEL(常時), 2:DIST(単発移動) | 
| 2-3 | Target V | int16 | 0.01m/s | 目標線速度 (例: 15 = 0.15m/s) | 
| 4-5 | Target W | int16 | 0.1deg/s | 目標角速度 (例: 300 = 30.0deg/s) | 
| 6-7 | Param A | int16 | - | Mode2時: 移動距離(cm) | 
| 8-9 | Param B | int16 | - | Mode2時: 回転角度(deg) | 
| 10 | Checksum| uint8 | - | Sum(0-9) & 0xFF | 
| 11 | Footer | uint8 | - | 0x0D (\r) |


### RP2350 -> Zero

Size: 18 Bytes 

| Byte | Field | Type | Unit | Description | 
|:---:|:---|:---:|:---:|:---| 
| 0 | Header | uint8 | - | 0xA5 | 
| 1 | Status | uint8 | - | 0:Ready, 1:Moving, 2:Error, 3:LowBatt | 
| 2-3 | Roll | int16 | 0.1 deg | 機体の傾き（横） | 
| 4-5 | Pitch | int16 | 0.1 deg | 機体の傾き（前後：FOLOの転倒検知用） | 
| 6-7 | Yaw | int16 | 0.1 deg | 現在の向き（方位） | 
| 8-11| Odom X | int32 | mm | 起動時からの累積移動量X | 
| 12-15| Odom Y | int32 | mm | 起動時からの累積移動量Y | 
| 16 | Checksum| uint8 | - | Sum(0-17) & 0xFF | 
| 17 | Footer | uint8 | - | 0x0A (\n) |
