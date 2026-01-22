# RobotHead - VL53L8CX センサー + カメラオーバーレイ

Raspberry Pi Zero 2W用のVL53L8CX距離センサーとカメラを使用したヒートマップオーバーレイプログラム

## ビルド方法

```bash
cd /home/ryo/work/RobotC/RobotHead/build
cmake ..
make
```

## Raspberry Piへのデプロイ

```bash
cd /home/ryo/work/RobotC/scripts
chmod +x deploy_head.sh
./deploy_head.sh raspberrypi.local pi
```

## Ubuntu PCで画像を表示する方法

### 方法1: X11フォワーディング（推奨：低遅延）

Ubuntu PC側で:
```bash
# X11フォワーディングでSSH接続
ssh -X pi@raspberrypi.local

# Raspberry Pi上で実行
cd /home/pi/robot_head
sudo -E DISPLAY=$DISPLAY XAUTHORITY=$XAUTHORITY ./robot_head
```

**注意**: `sudo -E` を使用して環境変数を保持し、`DISPLAY` と `XAUTHORITY` を明示的に指定します。

### 方法2: HTTPストリーミング（推奨：ブラウザで確認）

Raspberry Pi上で:
```bash
cd /home/pi/robot_head
sudo ./robot_head --stream
```

Ubuntu PCのブラウザで以下にアクセス:
```
http://raspberrypi.local:8080
```

画像を更新するにはページをリロードしてください。

### 方法3: VNC経由（オプション）

Raspberry Pi上でVNCサーバーを起動:
```bash
sudo raspi-config
# Interface Options -> VNC -> Enable

# Ubuntu PCからVNCビューアで接続
```

## トラブルシューティング

### X11フォワーディングが動作しない

1. Raspberry Pi側で `/etc/ssh/sshd_config` を確認:
   ```
   X11Forwarding yes
   X11UseLocalhost no
   ```

2. SSHサービスを再起動:
   ```bash
   sudo systemctl restart sshd
   ```

3. Ubuntu PC側で `xauth` がインストールされているか確認:
   ```bash
   sudo apt install xauth
   ```

### カメラが認識されない

```bash
# カメラデバイスの確認
ls -l /dev/video*

# カメラモジュールの有効化
sudo raspi-config
# Interface Options -> Camera -> Enable
```

### I2Cセンサーが認識されない

```bash
# I2Cの有効化
sudo raspi-config
# Interface Options -> I2C -> Enable

# デバイスの確認
sudo i2cdetect -y 1
```

## 終了方法

- X11モード: `q` キーを押す
- ストリーミングモード: `Ctrl+C` で終了
