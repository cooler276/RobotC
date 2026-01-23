# RobotC 各プロセッサAPI・通信プロトコル設計

## 概要
本ドキュメントは、RobotCの各プロセッサ（PC／Zero2W／Pico2）のAPI設計と、MQTT・UART通信プロトコル仕様をまとめたものです。

---

## 1. 各プロセッサのAPI設計

### PC（Robot Control / TTS）
- MQTTでZero2Wにコマンド送信・状態受信
- 主要API例：
    - `move(direction, speed)`
    - `speak(text)`
    - `set_emotion(emotion)`
    - `get_status()`
    - `get_map()`

### Raspberry Pi Zero 2W
- MQTTでPCと、UARTでPico2と通信
- 主要API例：
    - `detect_person()`
    - `get_distance()`
    - `play_audio(file)`
    - `set_led(color)`
    - `send_motor_command(rot, drive)`
    - `get_imu()`
    - `get_motor_status()`

### Raspberry Pi Pico 2
- UARTでZero2Wと通信
- 主要API例：
    - `set_motor(rot, drive)`
    - `get_imu()`
    - `stop_all()`

---

## 2. 通信プロトコル設計

### PC ⇔ Zero2W（MQTT）
- **MQTTブローカー（サーバー）はPC側で稼働し、PC・Zero2Wはクライアントとして接続する構成を推奨**
- **トピック例**
    - `robot/command`  ... PC→Zero2W コマンド
    - `robot/status`   ... Zero2W→PC 状態・センサデータ・アラート
    - `robot/audio`    ... PC→Zero2W 音声データ
    - `robot/map`      ... PC⇔Zero2W 地図・自己位置情報
- **メッセージ例（JSON）**
```json
{ "type": "move", "direction": "forward", "speed": 50 }
{ "type": "speak", "text": "こんにちは！" }
{ "type": "set_emotion", "emotion": "happy" }
{ "type": "stop" }
{ "type": "status", "battery": 82, "imu": [0.1, 0.2, 9.7], "obstacle": false }
{ "type": "alert", "reason": "fall" }
{ "type": "audio", "data": "<base64-encoded-audio>" }
```

### Zero2W ⇔ Pico2（UART）
- **コマンド例（ASCII）**
    - `R50` ... 回転モーター+50%
    - `D-30` ... 駆動モーター-30%
    - `S` ... 停止
    - `CALIB` ... IMUキャリブレーション開始
    - `RESET` ... 状態リセット/初期化
- **IMUデータ（CSV）**
    - `IMU,ax,ay,az,gx,gy,gz\n`
    - 例: `IMU,-0.98,0.04,9.73,-0.06,-0.09,-0.08`
- **異常通知・応答例**
    - `ALERT,FALL` ... 転倒検知
    - `ALERT,LIFT` ... 持ち上げ検知
    - `ACK,CALIB` ... キャリブレーション受付
    - `INFO,ver1.2.0` ... バージョン通知

---

## 3. 今後の拡張案
- MQTT QoS/Retain/認証設計
- UARTコマンドの拡張（例：エラー通知）
