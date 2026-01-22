#include <opencv2/opencv.hpp>
#include <iostream>
#include <fstream>

using namespace cv;
using namespace std;

// キャリブレーションパラメータ
struct CalibParams {
    int offset_x = 0;      // カメラ画像上でのdepth mapの左上X座標
    int offset_y = 0;      // カメラ画像上でのdepth mapの左上Y座標
    int overlay_width = 480;  // depth mapを重ねる幅
    int overlay_height = 480; // depth mapを重ねる高さ
    float alpha = 0.5f;    // ブレンド率（0.0=カメラのみ、1.0=depthのみ）
};

// パラメータをYAMLファイルに保存
void saveCalibration(const string& filename, const CalibParams& params) {
    FileStorage fs(filename, FileStorage::WRITE);
    fs << "offset_x" << params.offset_x;
    fs << "offset_y" << params.offset_y;
    fs << "overlay_width" << params.overlay_width;
    fs << "overlay_height" << params.overlay_height;
    fs << "alpha" << params.alpha;
    fs.release();
    cout << "キャリブレーション結果を保存しました: " << filename << endl;
}

// パラメータをYAMLファイルから読み込み
bool loadCalibration(const string& filename, CalibParams& params) {
    FileStorage fs(filename, FileStorage::READ);
    if (!fs.isOpened()) {
        return false;
    }
    fs["offset_x"] >> params.offset_x;
    fs["offset_y"] >> params.offset_y;
    fs["overlay_width"] >> params.overlay_width;
    fs["overlay_height"] >> params.overlay_height;
    fs["alpha"] >> params.alpha;
    fs.release();
    return true;
}

int main(int argc, char** argv) {
    string calib_file = "../Data/depth_calibration.yaml";
    
    if (argc > 1) {
        calib_file = argv[1];
    }

    // カメラの初期化
    VideoCapture cap("/dev/video0", cv::CAP_V4L2);
    if (!cap.isOpened()) {
        cerr << "カメラが見つかりません。" << endl;
        return -1;
    }

    cap.set(CAP_PROP_FRAME_WIDTH, 640);
    cap.set(CAP_PROP_FRAME_HEIGHT, 480);

    // キャリブレーションパラメータの初期化
    CalibParams params;
    if (loadCalibration(calib_file, params)) {
        cout << "既存のキャリブレーションを読み込みました" << endl;
    } else {
        cout << "新規キャリブレーションを開始します" << endl;
        // カメラが縦向きになっているので、初期値を調整
        params.offset_x = 0;
        params.offset_y = 100;
        params.overlay_width = 480;
        params.overlay_height = 480;
        params.alpha = 0.5f;
    }

    cout << "\n===== Depth Calibration Tool =====" << endl;
    cout << "操作方法:" << endl;
    cout << "  矢印キー: オーバーレイ位置の調整" << endl;
    cout << "  w/s: 幅の調整" << endl;
    cout << "  h/j: 高さの調整" << endl;
    cout << "  a/d: 透明度の調整" << endl;
    cout << "  Enter: 保存して終了" << endl;
    cout << "  ESC/q: 保存せずに終了" << endl;
    cout << "==================================\n" << endl;

    // ダミーのdepth mapを作成（実際のキャリブレーション時は8x8のdepthを使う）
    Mat dummy_depth(8, 8, CV_8UC1);
    for (int r = 0; r < 8; ++r) {
        for (int c = 0; c < 8; ++c) {
            dummy_depth.at<uchar>(r, c) = (r * 8 + c) * 4;
        }
    }
    Mat depth_color;
    applyColorMap(dummy_depth, depth_color, COLORMAP_JET);

    namedWindow("Calibration", WINDOW_AUTOSIZE);

    while (true) {
        Mat frame;
        cap >> frame;
        if (frame.empty()) {
            break;
        }

        // カメラ画像を左90度回転（縦向きに）
        rotate(frame, frame, ROTATE_90_COUNTERCLOCKWISE);

        // depth mapをリサイズ
        Mat depth_resized;
        resize(depth_color, depth_resized, 
               Size(params.overlay_width, params.overlay_height), 0, 0, INTER_NEAREST);

        // ROIを設定してオーバーレイ
        int x = max(0, min(params.offset_x, frame.cols - params.overlay_width));
        int y = max(0, min(params.offset_y, frame.rows - params.overlay_height));
        int w = min(params.overlay_width, frame.cols - x);
        int h = min(params.overlay_height, frame.rows - y);

        if (w > 0 && h > 0) {
            Rect roi(x, y, w, h);
            Mat depth_roi = depth_resized(Rect(0, 0, w, h));
            addWeighted(frame(roi), 1.0 - params.alpha, depth_roi, params.alpha, 0, frame(roi));
            
            // オーバーレイ範囲を矩形で表示
            rectangle(frame, roi, Scalar(0, 255, 0), 2);
        }

        // パラメータ表示
        string info = "Offset: (" + to_string(params.offset_x) + ", " + to_string(params.offset_y) + ") " +
                      "Size: " + to_string(params.overlay_width) + "x" + to_string(params.overlay_height) + " " +
                      "Alpha: " + to_string(params.alpha);
        putText(frame, info, Point(10, 30), FONT_HERSHEY_SIMPLEX, 0.5, Scalar(0, 255, 0), 1);

        imshow("Calibration", frame);

        int key = waitKey(30);
        if (key == 27 || key == 'q') { // ESC or q
            cout << "キャリブレーションをキャンセルしました" << endl;
            break;
        } else if (key == 13) { // Enter
            saveCalibration(calib_file, params);
            break;
        } else if (key == 82 || key == 0) { // Up arrow (Linux: 82 or special key code)
            params.offset_y -= 10;
        } else if (key == 84 || key == 1) { // Down arrow
            params.offset_y += 10;
        } else if (key == 81 || key == 2) { // Left arrow
            params.offset_x -= 10;
        } else if (key == 83 || key == 3) { // Right arrow
            params.offset_x += 10;
        } else if (key == 'w') {
            params.overlay_width += 10;
        } else if (key == 's') {
            params.overlay_width = max(10, params.overlay_width - 10);
        } else if (key == 'h') {
            params.overlay_height += 10;
        } else if (key == 'j') {
            params.overlay_height = max(10, params.overlay_height - 10);
        } else if (key == 'a') {
            params.alpha = max(0.0f, params.alpha - 0.1f);
        } else if (key == 'd') {
            params.alpha = min(1.0f, params.alpha + 0.1f);
        }
    }

    cap.release();
    destroyAllWindows();
    return 0;
}
