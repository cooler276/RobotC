#!/bin/bash
# Raspberry Piのカメラ状態確認スクリプト

echo "=========================================="
echo "Raspberry Pi カメラ診断"
echo "=========================================="
echo ""

# 1. カメラデバイスの確認
echo "1. カメラデバイスの確認:"
if [ -e /dev/video0 ]; then
    echo "✓ /dev/video0 が存在します"
    ls -la /dev/video0
else
    echo "✗ /dev/video0 が見つかりません"
fi
echo ""

# 2. v4l2-ctlコマンドでカメラ情報を確認
echo "2. カメラ情報（v4l2-ctl）:"
if command -v v4l2-ctl &> /dev/null; then
    v4l2-ctl --device=/dev/video0 --all 2>&1 | head -20
else
    echo "v4l2-ctl コマンドが見つかりません"
    echo "インストールするには: sudo apt install v4l-utils"
fi
echo ""

# 3. カメラモジュールのロード状態
echo "3. カメラモジュールのロード状態:"
lsmod | grep -E "bcm2835|unicam|v4l2"
echo ""

# 4. メモリ使用状況
echo "4. メモリ使用状況:"
free -h
echo ""

# 5. カメラ設定（/boot/config.txt）
echo "5. カメラ設定（/boot/config.txt または /boot/firmware/config.txt）:"
if [ -f /boot/config.txt ]; then
    grep -E "camera|^dtoverlay" /boot/config.txt | grep -v "^#"
elif [ -f /boot/firmware/config.txt ]; then
    grep -E "camera|^dtoverlay" /boot/firmware/config.txt | grep -v "^#"
else
    echo "config.txt が見つかりません"
fi
echo ""

# 6. GPU メモリ割り当て
echo "6. GPU メモリ割り当て:"
vcgencmd get_mem gpu 2>/dev/null || echo "vcgencmd コマンドが利用できません"
echo ""

echo "=========================================="
echo "診断完了"
echo "=========================================="
