#include <opencv2/opencv.hpp>
#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sstream>

using namespace cv;
using namespace std;

// グローバル変数
Mat g_current_frame;
mutex g_frame_mutex;
bool g_running = true;
vector<vector<Point2f>> g_image_points;
int g_capture_count = 0;
bool g_calibration_done = false;
string g_message = "チェッカーボードを様々な角度から撮影してください";
bool g_capture_requested = false;
bool g_calibrate_requested = false;

// チェッカーボードのサイズ（内部コーナー数）
const Size BOARD_SIZE(6, 9); // 7x10チェッカーボードの内部コーナー
const float SQUARE_SIZE = 25.0f; // 正方形のサイズ（mm）

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
    <title>Camera Calibration</title>
    <style>
        body { font-family: Arial; text-align: center; padding: 20px; }
        img { max-width: 90%; border: 2px solid #333; }
        button { font-size: 20px; margin: 10px; padding: 15px 30px; }
        .status { font-size: 18px; margin: 20px; }
    </style>
</head>
<body>
    <h1>Camera Calibration Tool</h1>
    <div class="status" id="status">Loading...</div>
    <img id="frame" src="/stream.jpg" alt="camera">
    <br>
    <button onclick="capture()">Capture (Count: <span id="count">0</span>)</button>
    <button onclick="calibrate()">Run Calibration</button>
    <button onclick="reset()">Reset</button>
    <script>
        function updateFrame() {
            document.getElementById("frame").src = "/stream.jpg?" + new Date().getTime();
        }
        function updateStatus() {
            fetch("/status").then(r => r.text()).then(text => {
                const parts = text.split("|");
                document.getElementById("status").innerText = parts[0];
                document.getElementById("count").innerText = parts[1];
            });
        }
        function capture() {
            fetch("/capture").then(r => r.text()).then(text => {
                alert(text);
                updateStatus();
            });
        }
        function calibrate() {
            if(confirm("Run calibration?")) {
                document.getElementById("status").innerText = "Processing...";
                fetch("/calibrate").then(r => r.text()).then(text => {
                    alert(text);
                    updateStatus();
                });
            }
        }
        function reset() {
            if(confirm("Reset all captures?")) {
                fetch("/reset").then(r => r.text()).then(text => {
                    alert(text);
                    updateStatus();
                });
            }
        }
        setInterval(updateFrame, 500);
        setInterval(updateStatus, 1000);
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
    server_addr.sin_port = htons(8080);

    bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr));
    listen(server_sock, 5);

    cout << "HTTPサーバー起動: http://0.0.0.0:8080/" << endl;

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
            string status_text = g_message + "|" + to_string(g_capture_count);
            vector<uchar> data(status_text.begin(), status_text.end());
            send_http_response(client_sock, "text/plain", data);
        } else if (request.find("GET /capture") == 0) {
            // キャプチャ処理をリクエスト
            g_capture_requested = true;
            string response = "撮影リクエストを受け付けました";
            vector<uchar> data(response.begin(), response.end());
            send_http_response(client_sock, "text/plain", data);
        } else if (request.find("GET /calibrate") == 0) {
            // キャリブレーション処理をリクエスト
            g_calibrate_requested = true;
            string response = "キャリブレーションリクエストを受け付けました";
            vector<uchar> data(response.begin(), response.end());
            send_http_response(client_sock, "text/plain", data);
        } else if (request.find("GET /reset") == 0) {
            lock_guard<mutex> lock(g_frame_mutex);
            g_image_points.clear();
            g_capture_count = 0;
            g_message = "リセットしました";
            string response = "OK";
            vector<uchar> data(response.begin(), response.end());
            send_http_response(client_sock, "text/plain", data);
        }

        close(client_sock);
    }

    close(server_sock);
}

int main() {
    VideoCapture cap("/dev/video0", cv::CAP_V4L2);
    if (!cap.isOpened()) {
        cerr << "カメラが見つかりません。" << endl;
        return -1;
    }

    cap.set(CAP_PROP_FRAME_WIDTH, 640);
    cap.set(CAP_PROP_FRAME_HEIGHT, 480);

    thread http_thread(http_server_thread);
    cout << "ブラウザで http://<RaspberryPiのIP>:8080/ を開いてください" << endl;

    while (g_running) {
        Mat frame;
        cap >> frame;
        if (frame.empty()) break;

        // キャリブレーションは回転前の画像で実行（main.cppのundistort適用と一致させる）
        Mat display = frame.clone();
        vector<Point2f> corners;
        bool found = findChessboardCorners(display, BOARD_SIZE, corners,
            CALIB_CB_ADAPTIVE_THRESH | CALIB_CB_NORMALIZE_IMAGE);

        if (found) {
            Mat gray;
            cvtColor(frame, gray, COLOR_BGR2GRAY);
            cornerSubPix(gray, corners, Size(11, 11), Size(-1, -1),
                TermCriteria(TermCriteria::EPS + TermCriteria::COUNT, 30, 0.1));
            drawChessboardCorners(display, BOARD_SIZE, corners, found);
            
            if (g_capture_requested) {
                lock_guard<mutex> lock(g_frame_mutex);
                g_image_points.push_back(corners);
                g_capture_count++;
                g_message = "撮影成功！ 合計" + to_string(g_capture_count) + "枚";
                g_capture_requested = false;
            }
        } else {
            putText(display, "Chessboard not found", Point(20, 50),
                FONT_HERSHEY_SIMPLEX, 1, Scalar(0, 0, 255), 2);
        }

        // Webストリーミング用に回転（表示のみ）
        rotate(display, display, ROTATE_90_COUNTERCLOCKWISE);

        if (g_calibrate_requested && g_capture_count >= 10) {
            lock_guard<mutex> lock(g_frame_mutex);
            
            // 3D点を生成
            vector<vector<Point3f>> object_points;
            vector<Point3f> obj;
            for (int i = 0; i < BOARD_SIZE.height; i++) {
                for (int j = 0; j < BOARD_SIZE.width; j++) {
                    obj.push_back(Point3f(j * SQUARE_SIZE, i * SQUARE_SIZE, 0));
                }
            }
            for (size_t i = 0; i < g_image_points.size(); i++) {
                object_points.push_back(obj);
            }

            Mat camera_matrix, dist_coeffs;
            vector<Mat> rvecs, tvecs;
            double rms = calibrateCamera(object_points, g_image_points, frame.size(),
                camera_matrix, dist_coeffs, rvecs, tvecs);

            // 結果を保存
            FileStorage fs("../Data/camera_calibration.yaml", FileStorage::WRITE);
            fs << "camera_matrix" << camera_matrix;
            fs << "distortion_coefficients" << dist_coeffs;
            fs << "image_width" << frame.cols;
            fs << "image_height" << frame.rows;
            fs << "rms_error" << rms;
            fs.release();

            g_message = "キャリブレーション完了！ RMS誤差: " + to_string(rms);
            g_calibration_done = true;
            g_calibrate_requested = false;
        } else if (g_calibrate_requested) {
            g_message = "撮影枚数が不足しています（最低10枚必要）";
            g_calibrate_requested = false;
        }

        {
            lock_guard<mutex> lock(g_frame_mutex);
            g_current_frame = display;
        }

        this_thread::sleep_for(chrono::milliseconds(100));
    }

    g_running = false;
    http_thread.join();
    cap.release();

    return 0;
}
