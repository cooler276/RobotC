#include <opencv2/opencv.hpp>
#include <iostream>

using namespace cv;
using namespace std;

int main() {
    cout << "Testing camera access..." << endl;
    
    // Method 1: Device path with V4L2
    VideoCapture cap1("/dev/video0", CAP_V4L2);
    if (cap1.isOpened()) {
        cout << "Method 1 SUCCESS: /dev/video0 with CAP_V4L2" << endl;
        cap1.release();
        return 0;
    }
    cout << "Method 1 FAILED: /dev/video0 with CAP_V4L2" << endl;
    
    // Method 2: Device index
    VideoCapture cap2(0);
    if (cap2.isOpened()) {
        cout << "Method 2 SUCCESS: index 0" << endl;
        cap2.release();
        return 0;
    }
    cout << "Method 2 FAILED: index 0" << endl;
    
    // Method 3: Device path without backend
    VideoCapture cap3("/dev/video0");
    if (cap3.isOpened()) {
        cout << "Method 3 SUCCESS: /dev/video0 without backend" << endl;
        cap3.release();
        return 0;
    }
    cout << "Method 3 FAILED: /dev/video0 without backend" << endl;
    
    cerr << "All methods failed!" << endl;
    return 1;
}
