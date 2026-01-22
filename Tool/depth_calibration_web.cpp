#include <opencv2/opencv.hpp>
#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sstream>
#include <cstring>

// VL53L8CXヘッダー
extern "C" {
    #include "vl53l8cx_api.h"
    #include "platform.h"
    #include "platform_wrapper.h"
}

using namespace cv;
using namespace std;

// グローバル変数
Mat g_current_frame;
mutex g_frame_mutex;
bool g_running = true;

// キャリブレーションパラメータ
int g_offset_x = 0;
int g_offset_y = 0;
int g_overlay_width = 240;
int g_overlay_height = 240;
float g_alpha = 0.5f;
string g_message = "Depthセンサーがカメラ画角外でもOK。位置を調整してください";

// HTTPレスポンスを送信
void send_http_response(int client_sock, const string& content_type, const vector<uchar>& data) {
    string header = 
        "HTTP/1.0 200 OK\r\n"
        "Content-Type: " + content_type + "\r\n"
        "Content-Length: " + to_string(data.size()) + "\r\n"
        "Connection: close\r\n\r\n";
    send(client_sock, header.c_str(), header.length(), 0);
    send(client_sock, data.data(), data.size(), 0);
}

void send_html(int client_sock) {
    string html = R"HTML(
<!DOCTYPE html>
<html>
<head>
    <title>Depth Calibration</title>
    <style>
        body { font-family: Arial; text-align: center; padding: 20px; background: #222; color: white; }
        img { max-width: 90%; border: 2px solid #666; }
        button { font-size: 16px; margin: 5px; padding: 10px 20px; }
        .controls { margin: 20px; }
        .status { font-size: 16px; margin: 20px; color: #4CAF50; }
        .param { display: inline-block; margin: 10px; }
    </style>
</head>
<body>
    <h1>Depth Overlay Calibration</h1>
    <div class="status" id="status">Loading...</div>
    <img id="frame" src="/stream.jpg" alt="camera">
    <div class="controls">
        <div class="param">
            <button onclick="adjust('offset_x', -5)">← X</button>
            <span id="offset_x">0</span>
            <button onclick="adjust('offset_x', 5)">X →</button>
        </div>
        <div class="param">
            <button onclick="adjust('offset_y', -5)">↑ Y</button>
            <span id="offset_y">0</span>
            <button onclick="adjust('offset_y', 5)">Y ↓</button>
        </div>
        <br>
        <div class="param">
            <button onclick="adjust('width', -10)">- Width</button>
            <span id="width">240</span>
            <button onclick="adjust('width', 10)">Width +</button>
        </div>
        <div class="param">
            <button onclick="adjust('height', -10)">- Height</button>
            <span id="height">240</span>
            <button onclick="adjust('height', 10)">Height +</button>
        </div>
        <br>
        <div class="param">
            <button onclick="adjust('alpha', -0.1)">- Alpha</button>
            <span id="alpha">0.5</span>
            <button onclick="adjust('alpha', 0.1)">Alpha +</button>
        </div>
    </div>
    <button onclick="save()" style="font-size: 20px; padding: 15px 40px;">Save Calibration</button>
    <button onclick="reset()">Reset</button>
    <script>
        function updateFrame() {
            document.getElementById("frame").src = "/stream.jpg?" + new Date().getTime();
        }
        function updateStatus() {
            fetch("/status").then(r => r.text()).then(text => {
                const parts = text.split("|");
                document.getElementById("status").innerText = parts[0];
                document.getElementById("offset_x").innerText = parts[1];
                document.getElementById("offset_y").innerText = parts[2];
                document.getElementById("width").innerText = parts[3];
                document.getElementById("height").innerText = parts[4];
                document.getElementById("alpha").innerText = parseFloat(parts[5]).toFixed(2);
            });
        }
        function adjust(param, delta) {
            fetch("/adjust?param=" + param + "&delta=" + delta)
                .then(r => r.text())
                .then(() => updateStatus());
        }
        function save() {
            if(confirm("Save current calibration?")) {
                fetch("/save").then(r => r.text()).then(text => {
                    alert(text);
                });
            }
        }
        function reset() {
            if(confirm("Reset to default?")) {
                fetch("/reset").then(r => r.text()).then(() => updateStatus());
            }
        }
        setInterval(updateFrame, 300);
        setInterval(updateStatus, 500);
    </script>
</body>
</html>
)HTML";
    vector<uchar> data(html.begin(), html.end());
    send_http_response(client_sock, "text/html", data);
}

void http_server_thread() {
    int server_sock = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(8081);

    bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr));
    listen(server_sock, 5);

    cout << "HTTPサーバー起動: http://0.0.0.0:8081/" << endl;

    while (g_running) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &client_len);
        
        if (client_sock < 0) continue;

        char buffer[1024] = {0};
        read(client_sock, buffer, sizeof(buffer));
        string request(buffer);

        if (request.find("GET / ") == 0 || request.find("GET /index.html") == 0) {
            send_html(client_sock);
        } else if (request.find("GET /stream.jpg") == 0) {
            lock_guard<mutex> lock(g_frame_mutex);
            if (!g_current_frame.empty()) {
                vector<uchar> buf;
                imencode(".jpg", g_current_frame, buf);
                send_http_response(client_sock, "image/jpeg", buf);
            }
        } else if (request.find("GET /status") == 0) {
            string status_text = g_message + "|" + 
                to_string(g_offset_x) + "|" + to_string(g_offset_y) + "|" +
                to_string(g_overlay_width) + "|" + to_string(g_overlay_height) + "|" +
                to_string(g_alpha);
            vector<uchar> data(status_text.begin(), status_text.end());
            send_http_response(client_sock, "text/plain", data);
        } else if (request.find("GET /adjust?") == 0) {
            size_t param_pos = request.find("param=");
            size_t delta_pos = request.find("delta=");
            if (param_pos != string::npos && delta_pos != string::npos) {
                string param = request.substr(param_pos + 6, request.find("&", param_pos) - param_pos - 6);
                string delta_str = request.substr(delta_pos + 6, request.find(" ", delta_pos) - delta_pos - 6);
                float delta = stof(delta_str);
                
                if (param == "offset_x") g_offset_x += (int)delta;
                else if (param == "offset_y") g_offset_y += (int)delta;
                else if (param == "width") g_overlay_width = max(50, g_overlay_width + (int)delta);
                else if (param == "height") g_overlay_height = max(50, g_overlay_height + (int)delta);
                else if (param == "alpha") g_alpha = max(0.0f, min(1.0f, g_alpha + delta));
            }
            string response = "OK";
            vector<uchar> data(response.begin(), response.end());
            send_http_response(client_sock, "text/plain", data);
        } else if (request.find("GET /save") == 0) {
            FileStorage fs("../Data/depth_calibration.yaml", FileStorage::WRITE);
            fs << "offset_x" << g_offset_x;
            fs << "offset_y" << g_offset_y;
            fs << "overlay_width" << g_overlay_width;
            fs << "overlay_height" << g_overlay_height;
            fs << "alpha" << g_alpha;
            fs.release();
            
            string response = "Calibration saved successfully!";
            vector<uchar> data(response.begin(), response.end());
            send_http_response(client_sock, "text/plain", data);
        } else if (request.find("GET /reset") == 0) {
            g_offset_x = 0;
            g_offset_y = 0;
            g_overlay_width = 240;
            g_overlay_height = 240;
            g_alpha = 0.5f;
            string response = "OK";
            vector<uchar> data(response.begin(), response.end());
            send_http_response(client_sock, "text/plain", data);
        }

        close(client_sock);
    }

    close(server_sock);
}

int main() {
    // VL53L8CXセンサー初期化
    platform_init_i2c("/dev/i2c-1", 0x52);
    
    VL53L8CX_Configuration sensor;
    memset(&sensor, 0, sizeof(sensor));
    sensor.platform.address = 0x52;
    
    VL53L8CX_ResultsData results;
    uint8_t status, isAlive;

    status = vl53l8cx_is_alive(&sensor, &isAlive);
    if (!isAlive || status) {
        cerr << "VL53L8CXセンサーが応答しません。" << endl;
        return -1;
    }

    status = vl53l8cx_init(&sensor);
    if (status) {
        cerr << "センサーの初期化に失敗しました。" << endl;
        return -1;
    }

    status = vl53l8cx_set_resolution(&sensor, VL53L8CX_RESOLUTION_8X8);
    if (status) {
        cerr << "解像度の設定に失敗しました。" << endl;
        return -1;
    }

    status = vl53l8cx_start_ranging(&sensor);

    // カメラ初期化
    VideoCapture cap("/dev/video0", cv::CAP_V4L2);
    if (!cap.isOpened()) {
        cerr << "カメラが見つかりません。" << endl;
        return -1;
    }

    cap.set(CAP_PROP_FRAME_WIDTH, 320);
    cap.set(CAP_PROP_FRAME_HEIGHT, 240);

    thread http_thread(http_server_thread);

    cout << "ブラウザで http://<RaspberryPiのIP>:8081/ を開いてください" << endl;

    // Depthマップを保持（データが来ない時も前回のを表示し続ける）
    Mat depth_map = Mat::zeros(8, 8, CV_16UC1);
    
    while (g_running) {
        Mat frame;
        cap >> frame;
        if (frame.empty()) break;

        // カメラを左90度回転
        rotate(frame, frame, ROTATE_90_COUNTERCLOCKWISE);

        // Depth データ取得（新しいデータがあれば更新）
        uint8_t isReady = 0;
        vl53l8cx_check_data_ready(&sensor, &isReady);
        
        if (isReady) {
            vl53l8cx_get_ranging_data(&sensor, &results);
            for (int i = 0; i < 64; i++) {
                int row = i / 8;
                int col = i % 8;
                depth_map.at<uint16_t>(row, col) = results.distance_mm[i];
            }
        }
        // データが来なくても前回のdepth_mapをそのまま使う

        // Depthマップを色付け（回転なし）
        // 固定範囲で正規化（200mm～2000mm）
        // 近い=赤(255)、遠い=青(0)になるように逆変換
        Mat depth_normalized = Mat::zeros(8, 8, CV_8UC1);
        for (int i = 0; i < 8; i++) {
            for (int j = 0; j < 8; j++) {
                uint16_t dist = depth_map.at<uint16_t>(i, j);
                // 近いほど高い値（赤）、遠いほど低い値（青）
                int val = (int)((2000.0 - dist) * 255.0 / 1800.0);
                val = max(0, min(255, val));  // 0-255にクリップ
                depth_normalized.at<uint8_t>(i, j) = (uint8_t)val;
            }
        }
        
        Mat depth_colored;
        applyColorMap(depth_normalized, depth_colored, COLORMAP_JET);
        
        // Depthマップをリサイズして重ね合わせ
        Mat depth_resized;
        resize(depth_colored, depth_resized, Size(g_overlay_width, g_overlay_height), 0, 0, INTER_NEAREST);
        
        Mat display = frame.clone();
        
        // オーバーレイ範囲を計算（範囲チェックを厳密に）
        int x1 = max(0, g_offset_x);
        int y1 = max(0, g_offset_y);
        int x2 = min(display.cols, g_offset_x + g_overlay_width);
        int y2 = min(display.rows, g_offset_y + g_overlay_height);
        
        int dx1 = max(0, -g_offset_x);
        int dy1 = max(0, -g_offset_y);
        int dx2 = dx1 + (x2 - x1);
        int dy2 = dy1 + (y2 - y1);
        
        // 有効な範囲かチェック（depthマップの範囲も確認）
        if (x2 > x1 && y2 > y1 && dx2 > dx1 && dy2 > dy1 &&
            dx2 <= depth_resized.cols && dy2 <= depth_resized.rows) {
            Mat roi = display(Rect(x1, y1, x2 - x1, y2 - y1));
            Mat depth_roi = depth_resized(Rect(dx1, dy1, dx2 - dx1, dy2 - dy1));
            addWeighted(roi, 1.0 - g_alpha, depth_roi, g_alpha, 0, roi);
        }
        
        // 枠を描画
        rectangle(display, Point(x1, y1), Point(x2, y2), Scalar(0, 255, 0), 2);
        
        {
            lock_guard<mutex> lock(g_frame_mutex);
            g_current_frame = display;
        }

        this_thread::sleep_for(chrono::milliseconds(100));
    }

    vl53l8cx_stop_ranging(&sensor);
    g_running = false;
    http_thread.join();
    cap.release();

    return 0;
}
