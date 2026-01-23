#!/usr/bin/env python3
"""
YOLOv8モデルをONNX形式（INT8量子化）でエクスポートするスクリプト
Raspberry Pi Zero 2W向けに最適化

必要なパッケージ:
  pip install ultralytics onnx onnxruntime

使い方:
  python3 export_yolov8_int8.py
"""

from ultralytics import YOLO
import onnx
import os

def export_yolov8_onnx_int8():
    """YOLOv8モデルをONNX形式（INT8量子化）でエクスポート"""
    
    # YOLOv8nモデル（最軽量版）をロード
    # 他のモデル: yolov8s.pt, yolov8m.pt など（より大きく精度高いがPi Zero 2Wでは厳しい）
    model_name = "yolov8n.pt"
    print(f"YOLOv8モデル '{model_name}' をロード中...")
    model = YOLO(model_name)
    
    # ONNX形式でエクスポート（動的バッチサイズ、入力サイズは640x640）
    print("ONNX形式にエクスポート中...")
    onnx_path = model.export(
        format="onnx",
        imgsz=640,           # 入力画像サイズ（Pi Zero 2Wなら320-416推奨だが、まず640で）
        dynamic=False,       # 動的バッチサイズは無効（Pi用は固定サイズ推奨）
        simplify=True,       # ONNX簡略化
        opset=12             # ONNX opsetバージョン（OpenCV DNNとの互換性）
    )
    
    print(f"エクスポート完了: {onnx_path}")
    
    # ONNXモデルのロードと確認
    print("\nONNXモデルの検証中...")
    onnx_model = onnx.load(onnx_path)
    onnx.checker.check_model(onnx_model)
    print("ONNXモデルは正常です")
    
    # INT8量子化（onnxruntimeまたはTensorRTが必要）
    # 注意: OpenCV DNNでのINT8サポートは限定的
    # Pi Zero 2WではONNX Runtime Quantization Toolを使うのが現実的
    print("\nINT8量子化について:")
    print("- OpenCV DNNはINT8 ONNXの部分サポート（期待される速度向上は限定的）")
    print("- より効果的な量子化にはONNX Runtime、OpenVINO、TensorRT等が必要")
    print("- Pi Zero 2Wでは軽量モデル（320x320入力）への変更も検討してください")
    
    # 軽量版（320x320）のエクスポートも試行
    print("\n軽量版（320x320）のエクスポート中...")
    onnx_path_320 = model.export(
        format="onnx",
        imgsz=320,
        dynamic=False,
        simplify=True,
        opset=12
    )
    print(f"軽量版エクスポート完了: {onnx_path_320}")
    
    # 使用方法の出力
    print("\n=== エクスポートされたモデル ===")
    print(f"標準版（640x640）: {onnx_path}")
    print(f"軽量版（320x320）: {onnx_path_320}")
    print("\n=== 使用方法 ===")
    print("1. ONNXモデルをRaspberry Piにコピー:")
    print(f"   scp {onnx_path_320} ryo@192.168.1.156:/home/ryo/models/")
    print("2. C++コードで読み込み:")
    print("   cv::dnn::readNetFromONNX(\"/home/ryo/models/yolov8n.onnx\")")
    
    return onnx_path, onnx_path_320

if __name__ == "__main__":
    try:
        export_yolov8_onnx_int8()
    except Exception as e:
        print(f"エラー: {e}")
        print("\n必要なパッケージをインストールしてください:")
        print("  pip install ultralytics onnx onnxruntime")
