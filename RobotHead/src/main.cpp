/**
 * @file main.cpp
 * @brief Main program for RobotHead - Camera-based object detection with voice interaction
 * @author RobotC Project
 * @date 2026-01-23
 * 
 * Features:
 * - libcamera-based camera capture
 * - YOLOv8 object detection
 * - VL53L8CX ToF distance sensing
 * - Audio playback (startup sound, greetings)
 * - Voice detection (optional)
 * - HTTP MJPEG streaming (--stream mode)
 */

#include <opencv2/opencv.hpp>
#include <opencv2/imgcodecs.hpp> // For imwrite
#include <opencv2/imgproc.hpp>  // For rotate
#include <iostream>
#include <thread>
#include <chrono>
#include <mutex>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include "platform/i2c_dev.h"
#include "platform/platform_wrapper.h"
#include "sensors/vl53l8cx_api.h"
#include "camera/libcamera_capture.h"
#include "audio/audio_player.h"
#include "audio/voice_detector.h"

#ifdef ENABLE_OBJECT_DETECTION
#include "detection/object_detector.h"
#endif

using namespace cv;
using namespace std;

// グローバル変数（HTTPストリーミング用）
Mat g_current_frame;
bool g_stream_mode = false;
bool g_frame_ready = false;
std::mutex g_frame_mutex;

// 挨拶音声管理用
bool g_greeting_played = false;
uint16_t g_min_distance = 4000;  // 最小距離（mm）

// HTTPレスポンスを送信する関数（MJPEGストリーミング、約5fps）
void send_http_response(int client_sock) {
    const char* boundary = "frame";

    string header =
        "HTTP/1.0 200 OK\r\n"
        "Connection: close\r\n"
        "Max-Age: 0\r\n"
        "Expires: 0\r\n"
        "Cache-Control: no-cache, private\r\n"
        "Pragma: no-cache\r\n"
        "Content-Type: multipart/x-mixed-replace; boundary=" + string(boundary) + "\r\n\r\n";

    if (send(client_sock, header.c_str(), header.length(), 0) < 0) {
        return;
    }

    // 約5fpsでフレームを送信
    while (g_stream_mode) {
        Mat local_frame;
        {
            lock_guard<mutex> lock(g_frame_mutex);
            if (!g_frame_ready || g_current_frame.empty()) {
                // まだフレームが用意されていなければ少し待って次へ
                // ロックはすぐ解放する
            } else {
                local_frame = g_current_frame.clone();
            }
        }

        if (local_frame.empty()) {
            this_thread::sleep_for(chrono::milliseconds(200));
            continue;
        }

        vector<uchar> buf;
        if (!imencode(".jpg", local_frame, buf)) {
            this_thread::sleep_for(chrono::milliseconds(200));
            continue;
        }

        string part_header =
            "--" + string(boundary) + "\r\n"
            "Content-Type: image/jpeg\r\n"
            "Content-Length: " + to_string(buf.size()) + "\r\n\r\n";

        if (send(client_sock, part_header.c_str(), part_header.length(), 0) < 0) {
            break;
        }
        if (send(client_sock, buf.data(), buf.size(), 0) < 0) {
            break;
        }

        const char* crlf = "\r\n";
        if (send(client_sock, crlf, strlen(crlf), 0) < 0) {
            break;
        }

        // 5fps 相当のウェイト
        this_thread::sleep_for(chrono::milliseconds(200));
    }

    string end_marker = "--" + string(boundary) + "--\r\n";
    send(client_sock, end_marker.c_str(), end_marker.length(), 0);
}

// HTTPサーバースレッド
void http_server_thread() {
    int server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0) {
        cerr << "ソケットの作成に失敗しました。" << endl;
        return;
    }

    int opt = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(8080);

    if (bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        cerr << "バインドに失敗しました。" << endl;
        close(server_sock);
        return;
    }

    if (listen(server_sock, 5) < 0) {
        cerr << "リッスンに失敗しました。" << endl;
        close(server_sock);
        return;
    }

    cout << "HTTPサーバーがポート8080で起動しました" << endl;

    while (g_stream_mode) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &client_len);
        
        if (client_sock >= 0) {
            send_http_response(client_sock);
            close(client_sock);
        }
    }

    close(server_sock);
}

int main(int argc, char** argv) {
    // コマンドライン引数の確認
    if (argc > 1 && string(argv[1]) == "--stream") {
        g_stream_mode = true;
        cout << "ストリーミングモードで起動します" << endl;
    }

    // 起動音を再生
    AudioPlayer audio_player;
    audio_player.init();
    audio_player.playFile("/home/ryo/work/voice/kidou.wav");
    cout << "起動音を再生しました" << endl;
    
    // 起動音再生を待つ（2秒）
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));

    // 音声検出器の初期化（起動音再生後）
    VoiceDetector voice_detector;
    bool voice_enabled = false;
    cout << "音声検出器を初期化中..." << endl;
    // 標準エラー出力を一時的に抑制
    int stderr_backup = dup(STDERR_FILENO);
    freopen("/dev/null", "w", stderr);
    
    voice_enabled = voice_detector.init();
    
    // 標準エラー出力を復元
    fflush(stderr);
    dup2(stderr_backup, STDERR_FILENO);
    close(stderr_backup);
    
    if (voice_enabled) {
        cout << "音声検出器を初期化しました" << endl;
    } else {
        cout << "音声検出器の初期化に失敗しました（音声検知は無効）" << endl;
    }

    // カメラキャリブレーションデータの読み込み
    Mat camera_matrix, dist_coeffs;
    bool use_camera_calib = false;
    FileStorage fs_camera("./Data/camera_calibration.yaml", FileStorage::READ);
    if (fs_camera.isOpened()) {
        fs_camera["camera_matrix"] >> camera_matrix;
        fs_camera["distortion_coefficients"] >> dist_coeffs;
        fs_camera.release();
        use_camera_calib = true;
        cout << "カメラキャリブレーションデータを読み込みました" << endl;
    } else {
        cout << "カメラキャリブレーションデータが見つかりません（歪み補正なし）" << endl;
    }

    // Depthキャリブレーションデータの読み込み
    int depth_offset_x = 35, depth_offset_y = 60;
    int depth_width = 240, depth_height = 240;
    float depth_alpha = 0.5f;
    bool use_depth_calib = false;
    FileStorage fs_depth("./Data/depth_calibration.yaml", FileStorage::READ);
    if (fs_depth.isOpened()) {
        fs_depth["offset_x"] >> depth_offset_x;
        fs_depth["offset_y"] >> depth_offset_y;
        fs_depth["overlay_width"] >> depth_width;
        fs_depth["overlay_height"] >> depth_height;
        fs_depth["alpha"] >> depth_alpha;
        fs_depth.release();
        use_depth_calib = true;
        cout << "Depthキャリブレーションデータを読み込みました" << endl;
        cout << "  Offset: (" << depth_offset_x << ", " << depth_offset_y << ")" << endl;
        cout << "  Size: " << depth_width << "x" << depth_height << endl;
    } else {
        cout << "Depthキャリブレーションデータが見つかりません（デフォルト表示）" << endl;
    }

    // 物体検出器の初期化
#ifdef ENABLE_OBJECT_DETECTION
    ObjectDetector* detector = nullptr;
    try {
        // YOLOv4-tinyモデルを使用（OpenCV DNNで確実に動作）
        detector = new ObjectDetector(
            "./Data/models/yolov4-tiny.weights",
            "./Data/models/coco.names",
            0.6  // 信頼度閾値（高めに設定してメモリ節約）
        );
    } catch (const exception& e) {
        cerr << "物体検出器の初期化に失敗しました: " << e.what() << endl;
        cerr << "物体検出なしで続行します" << endl;
    }
#endif

    // カメラの初期化（libcamera使用）
    LibCameraCapture cap(320, 240);
    if (!cap.isOpened()) {
        cerr << "カメラが見つかりません。" << endl;
        return -1;
    }

    // VL53L8CXセンサーの初期化
    const string i2c_device = "/dev/i2c-1";
    const uint8_t address = 0x29;

    platform_init_i2c(i2c_device.c_str(), 0x52);

    VL53L8CX_Configuration dev;
    memset(&dev, 0, sizeof(dev));
    dev.platform.address = 0x52;

    uint8_t alive = 0;
    uint8_t st = vl53l8cx_is_alive(&dev, &alive);
    if (st != VL53L8CX_STATUS_OK || !alive) {
        cerr << "VL53L8CXセンサーが応答しません。" << endl;
        return -1;
    }

    if (vl53l8cx_init(&dev) != VL53L8CX_STATUS_OK) {
        cerr << "VL53L8CXセンサーの初期化に失敗しました。" << endl;
        return -1;
    }

    if (vl53l8cx_set_resolution(&dev, VL53L8CX_RESOLUTION_8X8) != VL53L8CX_STATUS_OK) {
        cerr << "解像度の設定に失敗しました。" << endl;
        return -1;
    }

    if (vl53l8cx_start_ranging(&dev) != VL53L8CX_STATUS_OK) {
        cerr << "レンジングの開始に失敗しました。" << endl;
        return -1;
    }

    cout << "8x8レンジングを開始しました (Ctrl-Cで停止)" << endl;
    VL53L8CX_ResultsData results;
    bool has_depth = false; // 少なくとも1回は有効なDepthを受信したか

    // HTTPサーバースレッドを開始
    thread http_thread;
    if (g_stream_mode) {
        http_thread = thread(http_server_thread);
    }

    // カメラ側もおおよそ5fpsになるようにレート制御
    auto last_frame_time = chrono::steady_clock::now();

    while (true) {
        // 5fps目安のウェイト（前フレームから200ms経つまで待つ）
        auto now = chrono::steady_clock::now();
        auto elapsed = chrono::duration_cast<chrono::milliseconds>(now - last_frame_time);
        if (elapsed < chrono::milliseconds(200)) {
            this_thread::sleep_for(chrono::milliseconds(200) - elapsed);
        }
        last_frame_time = chrono::steady_clock::now();

        Mat frame;
        if (!cap.read(frame) || frame.empty()) {
            cerr << "カメラ画像の取得に失敗しました。" << endl;
            break;
        }

        // カメラ歪み補正を適用
        Mat undistorted;
        if (use_camera_calib) {
            cv::undistort(frame, undistorted, camera_matrix, dist_coeffs);
            frame = undistorted;
        }

        uint8_t ready = 0;
        if (vl53l8cx_check_data_ready(&dev, &ready) != VL53L8CX_STATUS_OK) {
            cerr << "データ準備状態の確認に失敗しました。" << endl;
            break;
        }

        if (ready) {
            if (vl53l8cx_get_ranging_data(&dev, &results) == VL53L8CX_STATUS_OK) {
                has_depth = true;

                // 最小距離を計算（人検出時の距離判定用）
                g_min_distance = 4000;
                for (int i = 0; i < 64; i++) {
                    if (results.target_status[i] == 5 || results.target_status[i] == 9) {
                        if (results.distance_mm[i] < g_min_distance) {
                            g_min_distance = results.distance_mm[i];
                        }
                    }
                }
            }
        }

        // 画面を左90度回転（反時計回り）して縦長にする
        rotate(frame, frame, ROTATE_90_COUNTERCLOCKWISE);

        // カメラ画像をベースにDepthマップをオーバーレイ
        Mat display = frame.clone();
        
        
        // 物体検出を実行（回転後の画像に対して）
#ifdef ENABLE_OBJECT_DETECTION
        if (detector != nullptr) {
            try {
                vector<DetectedObject> detections = detector->detect(display);
                detector->drawDetections(display, detections);
                
                // 人を検出したら距離判定して挨拶音声を再生
                bool person_detected = false;
                for (const auto& det : detections) {
                    if (det.class_name == "person") {
                        person_detected = true;
                        break;
                    }
                }
                
                if (person_detected && g_min_distance < 400 && !g_greeting_played && !audio_player.isPlaying()) {
                    cout << "人を検出しました（距離: " << g_min_distance << "mm） - 挨拶音声を再生します" << endl;
                    audio_player.playRandomGreeting();
                    g_greeting_played = true;
                }
                
                // 人がいなくなったらフラグをリセット（次回検出時に再生できるように）
                if (!person_detected) {
                    g_greeting_played = false;
                }
            } catch (const exception& e) {
                cerr << "物体検出エラー: " << e.what() << endl;
            }
        }
#endif
        
        if (has_depth && use_depth_calib) {
            // 8x8 depthデータの取得と変換
            Mat depth_map = Mat::zeros(8, 8, CV_16UC1);
            for (int i = 0; i < 64; i++) {
                int row = i / 8;
                int col = i % 8;
                depth_map.at<uint16_t>(row, col) = results.distance_mm[i];
            }

            // Depthマップの正規化（200mm〜2000mmの範囲、近いほど赤）
            Mat depth_norm = Mat::zeros(8, 8, CV_8UC1);
            for (int i = 0; i < 8; i++) {
                for (int j = 0; j < 8; j++) {
                    uint16_t dist = depth_map.at<uint16_t>(i, j);
                    // 近いほど高い値（赤）、遠いほど低い値（青）
                    int val = (int)((2000.0 - dist) * 255.0 / 1800.0);
                    val = max(0, min(255, val));  // 0-255にクリップ
                    depth_norm.at<uint8_t>(i, j) = (uint8_t)val;
                }
            }

            // キャリブレーションサイズにリサイズ
            Mat depth_resized;
            resize(depth_norm, depth_resized, Size(depth_width, depth_height), 0, 0, INTER_NEAREST);
            
            // カラーマップ適用
            Mat depth_colored;
            applyColorMap(depth_resized, depth_colored, COLORMAP_JET);

            // オーバーレイ範囲を計算
            int x1 = max(0, depth_offset_x);
            int y1 = max(0, depth_offset_y);
            int x2 = min(display.cols, depth_offset_x + depth_width);
            int y2 = min(display.rows, depth_offset_y + depth_height);
            
            int dx1 = max(0, -depth_offset_x);
            int dy1 = max(0, -depth_offset_y);
            int dx2 = dx1 + (x2 - x1);
            int dy2 = dy1 + (y2 - y1);
            
            // 有効な範囲かチェック
            if (x2 > x1 && y2 > y1 && dx2 > dx1 && dy2 > dy1 &&
                dx2 <= depth_colored.cols && dy2 <= depth_colored.rows) {
                Mat roi = display(Rect(x1, y1, x2 - x1, y2 - y1));
                Mat depth_roi = depth_colored(Rect(dx1, dy1, dx2 - dx1, dy2 - dy1));
                addWeighted(roi, 1.0 - depth_alpha, depth_roi, depth_alpha, 0, roi);
            }
        } else if (has_depth) {
            // キャリブレーションデータがない場合は従来の縦結合方式
            Mat heatmap_raw(8, 8, CV_16UC1, results.distance_mm);
            Mat heatmap = heatmap_raw.clone();
            rotate(heatmap, heatmap, ROTATE_180);

            Mat heatmap_norm, heatmap_resized, heatmap_color;
            double minVal = 0.0, maxVal = 0.0;
            minMaxLoc(heatmap, &minVal, &maxVal, nullptr, nullptr);

            if (maxVal > minVal) {
                Mat heatmap_f32;
                heatmap.convertTo(heatmap_f32, CV_32F);
                heatmap_f32 = (heatmap_f32 - static_cast<float>(minVal)) * (255.0f / static_cast<float>(maxVal - minVal));
                heatmap_f32.convertTo(heatmap_norm, CV_8UC1);
            } else {
                heatmap_norm = Mat::zeros(heatmap.size(), CV_8UC1);
            }

            int cam_w = frame.cols;
            resize(heatmap_norm, heatmap_resized, Size(cam_w, cam_w), 0, 0, INTER_NEAREST);
            applyColorMap(heatmap_resized, heatmap_color, COLORMAP_JET);
            vconcat(frame, heatmap_color, display);
        }

        if (g_stream_mode) {
            {
                lock_guard<mutex> lock(g_frame_mutex);
                g_current_frame = display.clone();
                g_frame_ready = true;
            }
        } else {
            imshow("Camera with Heatmap and Depth Map", display);
            if (waitKey(1) == 'q') {
                break;
            }
        }
        
        // 音声検知と相槌再生
        if (voice_enabled && voice_detector.detectVoice() && !audio_player.isPlaying()) {
            cout << "音声を検知しました - 相槌を再生します" << endl;
            audio_player.playRandomResponse();
        }
    }

    g_stream_mode = false;
    if (http_thread.joinable()) {
        http_thread.join();
    }

    cap.release();
    destroyAllWindows();
    vl53l8cx_stop_ranging(&dev);
    
    // 物体検出器のクリーンアップ
#ifdef ENABLE_OBJECT_DETECTION
    if (detector != nullptr) {
        delete detector;
    }
#endif

    return 0;
}
