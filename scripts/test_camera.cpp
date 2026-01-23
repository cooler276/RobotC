#include <opencv2/opencv.hpp>
#include <iostream>

using namespace cv;
using namespace std;

int main() {
    cout << "Testing camera..." << endl;
    
    // 方法1: デバイスパスとV4L2バックエンドを明示
    VideoCapture cap("/dev/video0", CAP_V4L2);
    
    if (!cap.isOpened()) {
        cerr << "Failed to open camera" << endl;
        return -1;
    }
    
    cout << "Camera opened successfully" << endl;
    
    cap.set(CAP_PROP_FRAME_WIDTH, 320);
    cap.set(CAP_PROP_FRAME_HEIGHT, 240);
    
    cout << "Reading frames..." << endl;
    
    for (int i = 0; i < 10; i++) {
        Mat frame;
        bool success = cap.read(frame);
        
        if (!success || frame.empty()) {
            cerr << "Frame " << i << ": FAILED" << endl;
        } else {
            cout << "Frame " << i << ": OK (" << frame.cols << "x" << frame.rows << ")" << endl;
        }
    }
    
    cap.release();
    cout << "Test completed" << endl;
    
    return 0;
}
