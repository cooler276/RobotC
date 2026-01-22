
#!/bin/bash

# RobotBody (Pico 2) ビルドスクリプト

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
PICO_SDK_PATH="${PROJECT_ROOT}/pico-sdk"
BUILD_DIR="${PROJECT_ROOT}/RobotBody/build"

echo "======================================"
echo "  RobotBody ビルド (Pico 2 / RP2350)"
echo "======================================"

# Pico SDKの確認
if [ ! -d "$PICO_SDK_PATH" ]; then
    echo "Pico SDKが見つかりません。クローン中..."
    cd "$PROJECT_ROOT"
    git clone https://github.com/raspberrypi/pico-sdk.git
    cd pico-sdk
    git submodule update --init
    echo "Pico SDK インストール完了"
fi

# ビルドディレクトリ作成
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# CMake設定
echo ""
echo ">>> CMake設定中..."
cmake .. -DPICO_BOARD=pico2

if [ $? -ne 0 ]; then
    echo ""
    echo "======================================"
    echo "  CMake設定失敗"
    echo "======================================"
    exit 1
fi

# ビルド
echo ""
echo ">>> ビルド中..."
make -j$(nproc)

if [ $? -eq 0 ]; then
    echo ""
    echo "======================================"
    echo "  ビルド完了！"
    echo "======================================"
    echo ""
    echo "生成されたファイル:"
    ls -lh robot_body.{elf,uf2,bin,hex} 2>/dev/null | awk '{print "  " $9 " (" $5 ")"}'
    echo ""
    echo "次のステップ:"
    echo "  bash deploy_body.sh  # SWD経由でデプロイ"
else
    echo ""
    echo "======================================"
    echo "  ビルド失敗"
    echo "======================================"
    exit 1
fi
