#!/bin/bash

# Raspberry Pi Zero 2Wへのツールデプロイスクリプト

# .envファイルから設定を読み込み
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
if [ -f "${SCRIPT_DIR}/.env" ]; then
    source "${SCRIPT_DIR}/.env"
fi

RASPI_TOOL_DIR="robot_tools"

echo "======================================"
echo "  Tool Deployment Script"
echo "======================================"

# ビルドディレクトリの確認
if [ ! -d "../Tool/build" ]; then
    echo "Error: Tool/build ディレクトリが見つかりません"
    echo "まず Tool をビルドしてください:"
    echo "  cd ../Tool"
    echo "  mkdir -p build && cd build"
    echo "  cmake .. && make"
    exit 1
fi

# 実行ファイルの確認
if [ ! -f "../Tool/build/camera_calibration_web" ]; then
    echo "Error: camera_calibration_web が見つかりません"
    echo "Tool をビルドしてください"
    exit 1
fi

echo "Raspberry Pi に接続中..."

# ディレクトリ作成
ssh ${RASPI_USER}@${RASPI_IP} "mkdir -p ~/${RASPI_TOOL_DIR}"

echo "ファイルを転送中..."

# 実行ファイルを転送
scp ../Tool/build/camera_calibration_web ${RASPI_USER}@${RASPI_IP}:~/${RASPI_TOOL_DIR}/
scp ../Tool/build/depth_calibration_web ${RASPI_USER}@${RASPI_IP}:~/${RASPI_TOOL_DIR}/
scp ../Tool/build/calibrate_depth ${RASPI_USER}@${RASPI_IP}:~/${RASPI_TOOL_DIR}/

# Data ディレクトリも転送（既存のキャリブレーションファイルがあれば）
ssh ${RASPI_USER}@${RASPI_IP} "mkdir -p ~/Data"
if [ -f "../Data/camera_calibration.yaml" ]; then
    scp ../Data/camera_calibration.yaml ${RASPI_USER}@${RASPI_IP}:~/Data/
fi
if [ -f "../Data/depth_calibration.yaml" ]; then
    scp ../Data/depth_calibration.yaml ${RASPI_USER}@${RASPI_IP}:~/Data/
fi

echo ""
echo "======================================"
echo "  デプロイ完了！"
echo "======================================"
echo ""
echo "使い方:"
echo ""
echo "1. カメラキャリブレーション (チェッカーボード 7x10 必要):"
echo "   ssh ${RASPI_USER}@${RASPI_IP}"
echo "   cd ${RASPI_TOOL_DIR}"
echo "   ./camera_calibration_web"
echo ""
echo "   ブラウザで http://${RASPI_IP}:8080/ を開く"
echo "   - チェッカーボードを様々な角度から撮影"
echo "   - 緑の線が表示されたら Capture ボタンを押す"
echo "   - 10枚以上撮影したら Run Calibration を押す"
echo "   - Data/camera_calibration.yaml が生成される"
echo ""
echo "2. Depthキャリブレーション:"
echo "   ssh ${RASPI_USER}@${RASPI_IP}"
echo "   cd ${RASPI_TOOL_DIR}"
echo "   ./depth_calibration_web"
echo ""
echo "   ブラウザで http://${RASPI_IP}:8081/ を開く"
echo "   - ← X / X → : Depthマップの水平位置を調整"
echo "   - ↑ Y / Y ↓ : Depthマップの垂直位置を調整"
echo "   - Width +/- : Depthマップの幅を調整"
echo "   - Height +/- : Depthマップの高さを調整"
echo "   - Alpha +/- : 透明度を調整 (0.0=透明, 1.0=不透明)"
echo "   - 調整完了後 Save Calibration で保存"
echo "   - Data/depth_calibration.yaml が生成される"
echo ""
echo "   ※Depthセンサーがカメラ画角の外にある場合:"
echo "   - オフセットをマイナス方向にも調整可能です"
echo "   - 緑の枠がカメラ画像内に入るように調整してください"
echo ""
echo "   Depthマップの色の意味:"
echo "   - 赤/オレンジ: 近い (約200-700mm)"
echo "   - 黄/緑: 中距離 (約700-1400mm)"
echo "   - 青: 遠い (約1400-2000mm)"
echo "   ※2m以上は青で表示されます"
echo ""
echo "======================================"
