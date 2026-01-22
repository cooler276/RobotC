#!/usr/bin/env python3
"""
IMUキャリブレーションツール
ロボットを各姿勢に置いて、IMUデータを収集し、座標軸マッピングを自動決定します
"""

import serial
import time
import numpy as np
from flask import Flask, render_template_string, jsonify, request
import json
import threading

app = Flask(__name__)

# UARTポート設定
UART_PORT = '/dev/ttyS0'
UART_BAUD = 115200

# キャリブレーションデータ
calibration_data = {
    'flat': [],      # 水平（Z軸が上）
    'front': [],     # 前傾（X軸が上）
    'back': [],      # 後傾（X軸が下）
    'left': [],      # 左傾（Y軸が上）
    'right': [],     # 右傾（Y軸が下）
    'upside': []     # 逆さま（Z軸が下）
}

# UART読み取り用
uart = None
latest_imu_data = None
data_lock = threading.Lock()

HTML_TEMPLATE = """
<!DOCTYPE html>
<html>
<head>
    <title>IMU Calibration</title>
    <meta charset="utf-8">
    <style>
        body {
            font-family: Arial, sans-serif;
            max-width: 800px;
            margin: 50px auto;
            padding: 20px;
            background: #f0f0f0;
        }
        .container {
            background: white;
            padding: 30px;
            border-radius: 10px;
            box-shadow: 0 2px 10px rgba(0,0,0,0.1);
        }
        h1 {
            color: #333;
            text-align: center;
        }
        .pose-section {
            margin: 20px 0;
            padding: 15px;
            background: #f9f9f9;
            border-radius: 5px;
            border-left: 4px solid #4CAF50;
        }
        .pose-section.collected {
            border-left-color: #2196F3;
            background: #e3f2fd;
        }
        .pose-name {
            font-weight: bold;
            font-size: 1.2em;
            margin-bottom: 10px;
        }
        .pose-description {
            color: #666;
            margin-bottom: 10px;
        }
        button {
            background: #4CAF50;
            color: white;
            border: none;
            padding: 10px 20px;
            border-radius: 5px;
            cursor: pointer;
            font-size: 1em;
            margin: 5px;
        }
        button:hover {
            background: #45a049;
        }
        button:disabled {
            background: #ccc;
            cursor: not-allowed;
        }
        .calculate-btn {
            background: #2196F3;
            font-size: 1.2em;
            padding: 15px 30px;
            display: block;
            width: 100%;
            margin-top: 30px;
        }
        .calculate-btn:hover {
            background: #0b7dda;
        }
        .imu-display {
            background: #263238;
            color: #00ff00;
            padding: 15px;
            border-radius: 5px;
            font-family: monospace;
            margin: 20px 0;
        }
        .result {
            background: #fff3cd;
            border: 2px solid #ffc107;
            padding: 20px;
            border-radius: 5px;
            margin-top: 20px;
            white-space: pre-wrap;
            font-family: monospace;
        }
        .status {
            text-align: center;
            padding: 10px;
            margin: 10px 0;
            border-radius: 5px;
        }
        .status.collecting {
            background: #fff3cd;
            color: #856404;
        }
        .status.success {
            background: #d4edda;
            color: #155724;
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>🤖 IMU Calibration Tool</h1>
        
        <div class="imu-display" id="imu-display">
            IMU: Waiting for data...
        </div>
        
        <div id="status"></div>
        
        <div id="poses">
            <div class="pose-section" data-pose="flat">
                <div class="pose-name">1️⃣ 水平 (Flat)</div>
                <div class="pose-description">ロボットを水平に置いてください（通常の状態）</div>
                <button onclick="collectData('flat')">データ収集</button>
                <span class="count">0 samples</span>
            </div>
            
            <div class="pose-section" data-pose="front">
                <div class="pose-name">2️⃣ 前傾 (Front Up)</div>
                <div class="pose-description">前側を上に立てかけてください（90度）</div>
                <button onclick="collectData('front')">データ収集</button>
                <span class="count">0 samples</span>
            </div>
            
            <div class="pose-section" data-pose="back">
                <div class="pose-name">3️⃣ 後傾 (Back Up)</div>
                <div class="pose-description">後ろ側を上に立てかけてください（90度）</div>
                <button onclick="collectData('back')">データ収集</button>
                <span class="count">0 samples</span>
            </div>
            
            <div class="pose-section" data-pose="left">
                <div class="pose-name">4️⃣ 左傾 (Left Up)</div>
                <div class="pose-description">左側を上に立てかけてください（90度）</div>
                <button onclick="collectData('left')">データ収集</button>
                <span class="count">0 samples</span>
            </div>
            
            <div class="pose-section" data-pose="right">
                <div class="pose-name">5️⃣ 右傾 (Right Up)</div>
                <div class="pose-description">右側を上に立てかけてください（90度）</div>
                <button onclick="collectData('right')">データ収集</button>
                <span class="count">0 samples</span>
            </div>
            
            <div class="pose-section" data-pose="upside">
                <div class="pose-name">6️⃣ 逆さま (Upside Down)</div>
                <div class="pose-description">ロボットを逆さまにしてください</div>
                <button onclick="collectData('upside')">データ収集</button>
                <span class="count">0 samples</span>
            </div>
        </div>
        
        <button class="calculate-btn" onclick="calculate()">🔍 キャリブレーション計算</button>
        
        <div id="result"></div>
    </div>
    
    <script>
        // IMUデータを定期的に更新
        setInterval(async () => {
            const response = await fetch('/imu_data');
            const data = await response.json();
            if (data.accel) {
                document.getElementById('imu-display').innerHTML = 
                    `Accel: X=${data.accel[0].toFixed(2)} Y=${data.accel[1].toFixed(2)} Z=${data.accel[2].toFixed(2)} m/s²<br>` +
                    `Gyro:  X=${data.gyro[0].toFixed(2)} Y=${data.gyro[1].toFixed(2)} Z=${data.gyro[2].toFixed(2)} rad/s`;
            }
        }, 100);
        
        // データ収集状況を更新
        setInterval(async () => {
            const response = await fetch('/status');
            const data = await response.json();
            for (const [pose, count] of Object.entries(data.counts)) {
                const section = document.querySelector(`[data-pose="${pose}"]`);
                if (section) {
                    section.querySelector('.count').textContent = `${count} samples`;
                    if (count > 0) {
                        section.classList.add('collected');
                    }
                }
            }
        }, 500);
        
        async function collectData(pose) {
            const statusDiv = document.getElementById('status');
            statusDiv.className = 'status collecting';
            statusDiv.textContent = '📊 データ収集中... (2秒)';
            
            const response = await fetch('/collect', {
                method: 'POST',
                headers: {'Content-Type': 'application/json'},
                body: JSON.stringify({pose: pose})
            });
            
            const result = await response.json();
            
            if (result.success) {
                statusDiv.className = 'status success';
                statusDiv.textContent = `✅ ${pose} のデータ収集完了 (${result.count} samples)`;
                setTimeout(() => statusDiv.textContent = '', 3000);
            } else {
                statusDiv.className = 'status';
                statusDiv.textContent = `❌ エラー: ${result.error}`;
            }
        }
        
        async function calculate() {
            const statusDiv = document.getElementById('status');
            statusDiv.className = 'status collecting';
            statusDiv.textContent = '🔍 キャリブレーション計算中...';
            
            const response = await fetch('/calculate', {method: 'POST'});
            const result = await response.json();
            
            const resultDiv = document.getElementById('result');
            
            if (result.success) {
                statusDiv.className = 'status success';
                statusDiv.textContent = '✅ キャリブレーション完了！';
                
                resultDiv.innerHTML = `<strong>キャリブレーション結果:</strong><br><br>${result.code}`;
            } else {
                statusDiv.className = 'status';
                statusDiv.textContent = `❌ エラー: ${result.error}`;
                resultDiv.textContent = '';
            }
        }
    </script>
</body>
</html>
"""

def uart_reader_thread():
    """UART読み取りスレッド"""
    global uart, latest_imu_data
    
    while True:
        try:
            if uart and uart.is_open:
                line = uart.readline().decode('utf-8', errors='ignore').strip()
                if line.startswith('IMU,'):
                    parts = line.split(',')
                    if len(parts) == 7:
                        with data_lock:
                            latest_imu_data = {
                                'accel': [float(parts[1]), float(parts[2]), float(parts[3])],
                                'gyro': [float(parts[4]), float(parts[5]), float(parts[6])]
                            }
            else:
                time.sleep(0.1)
        except Exception as e:
            print(f"UART read error: {e}")
            time.sleep(0.1)

@app.route('/')
def index():
    return render_template_string(HTML_TEMPLATE)

@app.route('/imu_data')
def imu_data():
    with data_lock:
        if latest_imu_data:
            return jsonify(latest_imu_data)
        else:
            return jsonify({'accel': None, 'gyro': None})

@app.route('/status')
def status():
    counts = {pose: len(data) for pose, data in calibration_data.items()}
    return jsonify({'counts': counts})

@app.route('/collect', methods=['POST'])
def collect():
    pose = request.json.get('pose')
    
    if pose not in calibration_data:
        return jsonify({'success': False, 'error': 'Invalid pose'})
    
    # 2秒間データ収集（20Hz × 2秒 = 40サンプル）
    samples = []
    start_time = time.time()
    
    while time.time() - start_time < 2.0:
        with data_lock:
            if latest_imu_data:
                samples.append(latest_imu_data.copy())
        time.sleep(0.05)
    
    if len(samples) < 10:
        return jsonify({'success': False, 'error': 'Insufficient data'})
    
    calibration_data[pose] = samples
    
    return jsonify({'success': True, 'count': len(samples)})

@app.route('/calculate', methods=['POST'])
def calculate():
    # 全ての姿勢のデータが揃っているか確認
    for pose, data in calibration_data.items():
        if len(data) == 0:
            return jsonify({'success': False, 'error': f'Missing data for {pose}'})
    
    # 各姿勢の平均加速度を計算
    avg_accel = {}
    for pose, samples in calibration_data.items():
        accel_sum = np.array([0.0, 0.0, 0.0])
        for sample in samples:
            accel_sum += np.array(sample['accel'])
        avg_accel[pose] = accel_sum / len(samples)
    
    print("\n=== Average Acceleration (m/s²) ===")
    for pose, accel in avg_accel.items():
        print(f"{pose:8s}: X={accel[0]:6.2f}, Y={accel[1]:6.2f}, Z={accel[2]:6.2f}")
    
    # 重力加速度（9.8 m/s²）の方向から座標軸を判定
    # flat: Z軸が+9.8付近
    # upside: Z軸が-9.8付近
    # front: X軸が+9.8付近
    # back: X軸が-9.8付近
    # left: Y軸が+9.8付近
    # right: Y軸が-9.8付近
    
    # どの生データ軸がどの論理軸に対応するか判定
    flat = avg_accel['flat']
    upside = avg_accel['upside']
    front = avg_accel['front']
    back = avg_accel['back']
    left = avg_accel['left']
    right = avg_accel['right']
    
    # Z軸の判定（flat時に最大の軸）
    z_axis = np.argmax(np.abs(flat))
    z_invert = flat[z_axis] < 0
    
    # X軸の判定（front時に最大の軸、Z軸を除く）
    front_abs = np.abs(front)
    front_abs[z_axis] = 0  # Z軸を除外
    x_axis = np.argmax(front_abs)
    x_invert = front[x_axis] < 0
    
    # Y軸の判定（残りの軸）
    y_axis = 3 - z_axis - x_axis  # 0,1,2の合計は3なので
    y_invert = left[y_axis] < 0
    
    # コード生成
    code = f"""// IMU座標軸キャリブレーション結果
// 各姿勢での加速度:
//   flat:   X={flat[0]:6.2f}, Y={flat[1]:6.2f}, Z={flat[2]:6.2f}
//   front:  X={front[0]:6.2f}, Y={front[1]:6.2f}, Z={front[2]:6.2f}
//   back:   X={back[0]:6.2f}, Y={back[1]:6.2f}, Z={back[2]:6.2f}
//   left:   X={left[0]:6.2f}, Y={left[1]:6.2f}, Z={left[2]:6.2f}
//   right:  X={right[0]:6.2f}, Y={right[1]:6.2f}, Z={right[2]:6.2f}
//   upside: X={upside[0]:6.2f}, Y={upside[1]:6.2f}, Z={upside[2]:6.2f}

// main.cppに以下を追加:
imu.setAxisMapping({x_axis}, {y_axis}, {z_axis}, {str(x_invert).lower()}, {str(y_invert).lower()}, {str(z_invert).lower()});
"""
    
    print("\n" + code)
    
    # ファイルに保存
    with open('/tmp/imu_calibration.txt', 'w') as f:
        f.write(code)
    
    return jsonify({'success': True, 'code': code})

def main():
    global uart
    
    print("IMU Calibration Tool")
    print("====================")
    print(f"Opening UART: {UART_PORT} @ {UART_BAUD}")
    
    # UART初期化
    try:
        uart = serial.Serial(UART_PORT, UART_BAUD, timeout=0.1)
        print("✓ UART opened")
    except Exception as e:
        print(f"✗ Failed to open UART: {e}")
        return
    
    # UART読み取りスレッド開始
    reader = threading.Thread(target=uart_reader_thread, daemon=True)
    reader.start()
    print("✓ UART reader thread started")
    
    print("\nWebサーバー起動中...")
    print("ブラウザで http://192.168.1.156:5000 を開いてください")
    print("または http://localhost:5000")
    
    app.run(host='0.0.0.0', port=5000, debug=False)

if __name__ == '__main__':
    main()
