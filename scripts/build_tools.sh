#!/bin/bash

# Raspberry Pi Zero 2W 用のクロスコンパイルビルドスクリプト

echo "======================================"
echo "  Tool Cross-Compile Build Script"
echo "======================================"

# ビルドディレクトリのクリーンアップと作成
cd ../Tool
rm -rf build
mkdir -p build
cd build

echo "クロスコンパイル中..."

# CMakeでビルド（クロスコンパイル設定はCMakeLists.txtに含まれています）
cmake .. -DCMAKE_BUILD_TYPE=Release

# ビルド
make -j$(nproc)

if [ $? -eq 0 ]; then
    echo ""
    echo "======================================"
    echo "  ビルド成功！"
    echo "======================================"
    echo ""
    echo "次のコマンドでデプロイできます:"
    echo "  cd ../../scripts"
    echo "  bash deploy_tools.sh"
else
    echo ""
    echo "======================================"
    echo "  ビルド失敗"
    echo "======================================"
    exit 1
fi
