#!/usr/bin/env python3
"""
picamera2を使ってHTTPストリーミングを提供する簡易サーバー
C++プログラムからカメラ機能を分離
"""
from picamera2 import Picamera2
from flask import Flask, Response
import io
import time
from PIL import Image

app = Flask(__name__)

# カメラ初期化
picam2 = Picamera2()
config = picam2.create_video_configuration(
    main={"size": (320, 240), "format": "RGB888"}
)
picam2.configure(config)
picam2.start()
time.sleep(2)  # ウォームアップ

def generate_frames():
    """MJPEGストリーミング用のフレーム生成"""
    while True:
        # フレームをキャプチャ
        frame = picam2.capture_array()
        
        # PIL Imageに変換して90度回転
        img = Image.fromarray(frame)
        img = img.transpose(Image.ROTATE_270)  # 反時計回り90度
        
        # JPEGエンコード
        buffer = io.BytesIO()
        img.save(buffer, format='JPEG', quality=80)
        frame_bytes = buffer.getvalue()
        
        # MJPEGフレームとして送信
        yield (b'--frame\r\n'
               b'Content-Type: image/jpeg\r\n\r\n' + frame_bytes + b'\r\n')
        
        time.sleep(0.1)  # 約10fps

@app.route('/')
def index():
    return """
    <html>
    <head><title>Robot Camera</title></head>
    <body>
    <h1>Robot Head Camera Stream</h1>
    <img src="/video_feed" width="640" height="480">
    </body>
    </html>
    """

@app.route('/video_feed')
def video_feed():
    return Response(generate_frames(),
                    mimetype='multipart/x-mixed-replace; boundary=frame')

if __name__ == '__main__':
    try:
        print("Starting camera HTTP server on port 8080...")
        app.run(host='0.0.0.0', port=8080, threaded=True)
    finally:
        picam2.stop()
        picam2.close()
