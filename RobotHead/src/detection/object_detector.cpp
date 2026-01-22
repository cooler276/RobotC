#include "object_detector.h"
#include <fstream>
#include <iostream>

// OpenCV 4.xのDNN名前空間を使用
using namespace cv;
using namespace cv::dnn;

ObjectDetector::ObjectDetector(const string& model_path, const string& labels_path, float conf_threshold)
    : confidence_threshold_(conf_threshold) {
    
    // モデルを読み込み
    cout << "物体検出モデルを読み込み中..." << endl;
    
    // 拡張子でモデルタイプを判定
    if (model_path.find(".weights") != string::npos) {
        // Darknet (YOLO) モデル
        string cfg = model_path;
        cfg.replace(cfg.find(".weights"), 8, ".cfg");
        net_ = readNetFromDarknet(cfg, model_path);
    } else if (model_path.find(".tflite") != string::npos) {
        // TensorFlow Liteモデル
        net_ = readNetFromTensorflow(model_path);
    } else if (model_path.find(".pb") != string::npos) {
        // TensorFlowモデル（frozen graph）
        net_ = readNetFromTensorflow(model_path);
    } else if (model_path.find(".caffemodel") != string::npos) {
        // Caffeモデル
        string prototxt = model_path;
        prototxt.replace(prototxt.find(".caffemodel"), 12, ".prototxt");
        net_ = readNetFromCaffe(prototxt, model_path);
    } else {
        net_ = readNetFromTensorflow(model_path);
    }
    
    if (net_.empty()) {
        cerr << "エラー: モデルの読み込みに失敗しました: " << model_path << endl;
        throw runtime_error("Failed to load model");
    }
    
    // CPUバックエンドを使用（Raspberry Pi Zero 2Wでは最適）
    net_.setPreferableBackend(DNN_BACKEND_OPENCV);
    net_.setPreferableTarget(DNN_TARGET_CPU);
    
    // ラベルを読み込み
    loadLabels(labels_path);
    
    cout << "物体検出モデルの読み込みが完了しました（" << class_names_.size() << "クラス）" << endl;
}

ObjectDetector::~ObjectDetector() {
}

void ObjectDetector::loadLabels(const string& labels_path) {
    ifstream file(labels_path);
    if (!file.is_open()) {
        cerr << "エラー: ラベルファイルが開けません: " << labels_path << endl;
        return;
    }
    
    string line;
    while (getline(file, line)) {
        class_names_.push_back(line);
    }
    file.close();
}

vector<DetectedObject> ObjectDetector::detect(const Mat& frame) {
    vector<DetectedObject> detections;
    
    if (frame.empty()) {
        return detections;
    }
    
    // YOLOv4-tiny用の入力処理（メモリ節約のため320x320にリサイズ）
    Mat blob = blobFromImage(frame, 1/255.0, Size(320, 320), Scalar(0, 0, 0), true, false);
    
    // モデルに入力
    net_.setInput(blob);
    
    // 出力レイヤー名を取得
    vector<String> output_names = net_.getUnconnectedOutLayersNames();
    
    // 推論を実行
    vector<Mat> outputs;
    net_.forward(outputs, output_names);
    
    // YOLOの結果を解析
    vector<int> class_ids;
    vector<float> confidences;
    vector<Rect> boxes;
    
    for (size_t i = 0; i < outputs.size(); ++i) {
        float* data = (float*)outputs[i].data;
        for (int j = 0; j < outputs[i].rows; ++j, data += outputs[i].cols) {
            Mat scores = outputs[i].row(j).colRange(5, outputs[i].cols);
            Point class_id_point;
            double confidence;
            minMaxLoc(scores, 0, &confidence, 0, &class_id_point);
            
            if (confidence > confidence_threshold_) {
                int center_x = (int)(data[0] * frame.cols);
                int center_y = (int)(data[1] * frame.rows);
                int width = (int)(data[2] * frame.cols);
                int height = (int)(data[3] * frame.rows);
                int left = center_x - width / 2;
                int top = center_y - height / 2;
                
                class_ids.push_back(class_id_point.x);
                confidences.push_back((float)confidence);
                boxes.push_back(Rect(left, top, width, height));
            }
        }
    }
    
    // Non-Maximum Suppression（重複検出の削除）
    vector<int> indices;
    dnn::NMSBoxes(boxes, confidences, confidence_threshold_, 0.4, indices);
    
    // 検出結果を追加
    for (size_t i = 0; i < indices.size(); ++i) {
        int idx = indices[i];
        DetectedObject obj;
        obj.class_id = class_ids[idx];
        obj.class_name = (obj.class_id >= 0 && obj.class_id < static_cast<int>(class_names_.size()))
                         ? class_names_[obj.class_id]
                         : "unknown";
        obj.confidence = confidences[idx];
        obj.bbox = boxes[idx];
        detections.push_back(obj);
    }
    
    return detections;
}

void ObjectDetector::drawDetections(Mat& frame, const vector<DetectedObject>& detections) {
    for (const auto& obj : detections) {
        // バウンディングボックスを描画
        rectangle(frame, obj.bbox, Scalar(0, 255, 0), 2);
        
        // ラベルと信頼度を描画
        string label = obj.class_name + ": " + to_string(static_cast<int>(obj.confidence * 100)) + "%";
        
        int baseline = 0;
        Size label_size = getTextSize(label, FONT_HERSHEY_SIMPLEX, 0.5, 1, &baseline);
        
        // ラベル背景
        Point label_pos(obj.bbox.x, obj.bbox.y - 5);
        if (label_pos.y < 0) label_pos.y = 0;
        
        rectangle(frame, 
                  Rect(label_pos, Size(label_size.width, label_size.height + baseline)),
                  Scalar(0, 255, 0), FILLED);
        
        // ラベルテキスト
        putText(frame, label, Point(label_pos.x, label_pos.y + label_size.height),
                FONT_HERSHEY_SIMPLEX, 0.5, Scalar(0, 0, 0), 1);
    }
}
