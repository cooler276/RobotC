#!/bin/bash

# Raspberry Pi Zero 2W デプロイスクリプト
# 使い方: ./deploy_head.sh [raspberry_pi_ip] [user]

# .envファイルから設定を読み込み
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
if [ -f "${SCRIPT_DIR}/.env" ]; then
    source "${SCRIPT_DIR}/.env"
fi

# コマンドライン引数で上書き可能
RASPI_IP="${1:-${RASPI_IP}}"
RASPI_USER="${2:-${RASPI_USER}}"
REMOTE_DIR="/home/${RASPI_USER}/robot_head"
BUILD_DIR="../RobotHead/build"
BINARY_NAME="robot_head"

echo "=========================================="
echo "Raspberry Pi Zero 2W デプロイスクリプト"
echo "=========================================="
echo "ターゲット: ${RASPI_USER}@${RASPI_IP}"
echo "リモートディレクトリ: ${REMOTE_DIR}"
echo ""

# ビルドディレクトリの確認
if [ ! -f "${BUILD_DIR}/${BINARY_NAME}" ]; then
    echo "エラー: 実行ファイルが見つかりません: ${BUILD_DIR}/${BINARY_NAME}"
    echo "先にビルドを実行してください:"
    echo "  cd ../RobotHead/build && cmake .. && make"
    exit 1
fi

echo "✓ 実行ファイルを確認しました"

# Raspberry Piへの接続確認
echo "Raspberry Piへの接続を確認中..."
if ! ping -c 1 -W 2 ${RASPI_IP} > /dev/null 2>&1; then
    echo "警告: ${RASPI_IP} への ping が失敗しました"
    echo "続行しますか? (y/N)"
    read -r response
    if [[ ! "$response" =~ ^[Yy]$ ]]; then
        exit 1
    fi
fi

# リモートディレクトリの作成
echo "リモートディレクトリを作成中..."
ssh ${RASPI_USER}@${RASPI_IP} "mkdir -p ${REMOTE_DIR}"

if [ $? -ne 0 ]; then
    echo "エラー: SSHでの接続に失敗しました"
    echo "SSHキーが設定されているか、パスワード認証が有効か確認してください"
    exit 1
fi

# 実行ファイルの転送
echo "実行ファイルを転送中..."
rsync -avz --progress ${BUILD_DIR}/${BINARY_NAME} ${RASPI_USER}@${RASPI_IP}:${REMOTE_DIR}/

if [ $? -ne 0 ]; then
    echo "エラー: ファイルの転送に失敗しました"
    exit 1
fi

# Dataディレクトリの転送（rsyncで差分のみ転送）
echo "Dataディレクトリを同期中（差分のみ転送）..."
rsync -avz --progress --checksum ../Data/ ${RASPI_USER}@${RASPI_IP}:${REMOTE_DIR}/Data/

if [ $? -ne 0 ]; then
    echo "警告: Dataディレクトリの同期に失敗しました"
fi

# YOLOv8 ONNXモデルとラベルファイルの転送
echo "YOLOv8モデルとラベルファイルを転送中..."
if [ -f "${SCRIPT_DIR}/yolov8n_320.onnx" ]; then
    rsync -avz --progress ${SCRIPT_DIR}/yolov8n_320.onnx ${RASPI_USER}@${RASPI_IP}:${REMOTE_DIR}/Data/
    echo "✓ YOLOv8モデル（320x320）を転送しました"
fi
if [ -f "${SCRIPT_DIR}/coco.names" ]; then
    rsync -avz --progress ${SCRIPT_DIR}/coco.names ${RASPI_USER}@${RASPI_IP}:${REMOTE_DIR}/Data/
    echo "✓ COCOラベルファイルを転送しました"
fi

# 実行権限の付与
echo "実行権限を設定中..."
ssh ${RASPI_USER}@${RASPI_IP} "chmod +x ${REMOTE_DIR}/${BINARY_NAME}"

echo ""
echo "=========================================="
echo "デプロイが完了しました！"
echo "=========================================="
echo ""
echo "【方法1】X11フォワーディングで画像をUbuntu PCに表示:"
echo "  ssh -X ${RASPI_USER}@${RASPI_IP}"
echo "  cd ${REMOTE_DIR}"
echo "  sudo -E DISPLAY=\$DISPLAY XAUTHORITY=\$XAUTHORITY ./${BINARY_NAME}"
echo ""
echo "【方法2】HTTPストリーミングで画像をブラウザに表示:"
echo "  (プログラムを --stream モードで起動する必要があります)"
echo "  ssh ${RASPI_USER}@${RASPI_IP} 'cd ${REMOTE_DIR} && sudo ./${BINARY_NAME} --stream'"
echo "  ブラウザで http://${RASPI_IP}:8080 を開く"
echo ""
