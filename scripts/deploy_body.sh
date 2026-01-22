#!/bin/bash

# Raspberry Pi経由でPico 2にSWDデプロイ

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${PROJECT_ROOT}/RobotBody/build"
ELF_FILE="${BUILD_DIR}/robot_body.elf"

# .envファイルから設定を読み込み
if [ -f "${SCRIPT_DIR}/.env" ]; then
    source "${SCRIPT_DIR}/.env"
fi

# コマンドライン引数で上書き可能
RASPI_IP=${1:-${RASPI_IP}}
RASPI_USER=${2:-${RASPI_USER}}

if [ -z "$RASPI_IP" ] || [ -z "$RASPI_USER" ]; then
    echo "エラー: RASPI_IPとRASPI_USERが設定されていません"
    echo "使い方: $0 [RASPI_IP] [RASPI_USER]"
    echo "または .env ファイルを作成してください"
    exit 1
fi

echo "======================================"
echo "  Pico 2 デプロイ (SWD経由)"
echo "======================================"

# ELFファイルの存在確認
if [ ! -f "$ELF_FILE" ]; then
    echo "エラー: $ELF_FILE が見つかりません"
    echo "先にビルドを実行してください: bash build_body.sh"
    exit 1
fi

echo "実行ファイル: $ELF_FILE ($(stat -c%s "$ELF_FILE" | numfmt --to=iec-i --suffix=B))"
echo "Raspberry Pi: ${RASPI_USER}@${RASPI_IP}"

# Raspberry Piに転送
echo ""
echo ">>> Raspberry Piに転送中..."
scp "$ELF_FILE" "${RASPI_USER}@${RASPI_IP}:/tmp/robot_body.elf"

if [ $? -ne 0 ]; then
    echo "エラー: ファイル転送に失敗しました"
    exit 1
fi

echo ">>> 転送完了"

# OpenOCD設定ファイルを確認
OPENOCD_CFG="${PROJECT_ROOT}/Doc/raspberrypi-swd.cfg"
if [ -f "$OPENOCD_CFG" ]; then
    echo ">>> OpenOCD設定を転送中..."
    scp "$OPENOCD_CFG" "${RASPI_USER}@${RASPI_IP}:/tmp/raspberrypi-swd.cfg"
fi

# Raspberry Pi上でOpenOCD経由で書き込み
echo ""
echo ">>> OpenOCDでPico 2に書き込み中..."
ssh "${RASPI_USER}@${RASPI_IP}" << 'EOF'
    # OpenOCDでフラッシュ書き込み
    sudo openocd -f /tmp/raspberrypi-swd.cfg -f target/rp2350.cfg \
        -c "adapter speed 1000" \
        -c "init" \
        -c "targets" \
        -c "reset halt" \
        -c "flash write_image erase /tmp/robot_body.elf" \
        -c "verify_image /tmp/robot_body.elf" \
        -c "reset run" \
        -c "shutdown"
    
    if [ $? -eq 0 ]; then
        echo ""
        echo "======================================"
        echo "  デプロイ完了！"
        echo "======================================"
        echo ""
        echo "UARTで接続するには:"
        echo "  sudo cat /dev/ttyS0"
        echo "または:"
        echo "  sudo minicom -D /dev/ttyS0 -b 115200"
    else
        echo ""
        echo "======================================"
        echo "  デプロイ失敗"
        echo "======================================"
        echo ""
        echo "トラブルシューティング:"
        echo "1. SWD接続を確認 (GPIO24=SWCLK, GPIO25=SWDIO)"
        echo "2. Pico 2の電源を確認"
        echo "3. OpenOCDがインストールされているか確認:"
        echo "   sudo apt install openocd"
        exit 1
    fi
EOF

if [ $? -eq 0 ]; then
    echo ""
    echo "次のステップ:"
    echo "  1. sudo stty -F /dev/ttyS0 raw 115200 (raw mode設定)"
    echo "  2. sudo cat /dev/ttyS0 でIMUデータ確認 (20Hz)"
    echo "  3. モーターコマンド: echo \"R50\" | sudo tee /dev/ttyS0"
else
    echo ""
    echo "エラー: リモート実行に失敗しました"
    exit 1
fi
