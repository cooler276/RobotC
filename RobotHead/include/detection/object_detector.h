#ifndef OBJECT_DETECTOR_H
#define OBJECT_DETECTOR_H

#include <opencv2/opencv.hpp>
#include <opencv2/dnn.hpp>
#include <vector>
#include <string>

using namespace cv;
using namespace cv::dnn;
using namespace std;

struct DetectedObject {
    int class_id;
    string class_name;
    float confidence;
    Rect bbox;
};

class ObjectDetector {
public:
    ObjectDetector(const string& model_path, const string& labels_path, float conf_threshold = 0.5);
    ~ObjectDetector();
    
    vector<DetectedObject> detect(const Mat& frame);
    void drawDetections(Mat& frame, const vector<DetectedObject>& detections);
    
private:
    Net net_;
    vector<string> class_names_;
    float confidence_threshold_;
    
    void loadLabels(const string& labels_path);
};

#endif // OBJECT_DETECTOR_H
