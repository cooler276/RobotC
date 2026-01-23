#include "object_detector.h"
#include <fstream>
#include <iostream>

// OpenCV 4.xのDNN名前空間を使用
using namespace cv;
using namespace cv::dnn;

ObjectDetector::ObjectDetector(const string& model_path, const string& labels_path, float conf_threshold)
    : confidence_threshold_(conf_threshold), is_yolov8_(false), input_size_(640) {
    
    // モデルを読み込み
    cout << "物体検出モデルを読み込み中..." << endl;
    
    // 拡張子でモデルタイプを判定
    if (model_path.find(".onnx") != string::npos) {
        // ONNX (YOLOv8など) モデル
        net_ = readNetFromONNX(model_path);
        is_yolov8_ = true;
        
        // ファイル名から入力サイズを推定（例: yolov8n_320.onnxなら320）
        if (model_path.find("_320") != string::npos) {
            input_size_ = 320;
        } else if (model_path.find("_416") != string::npos) {
            input_size_ = 416;
        } else {
            input_size_ = 640;
        }
        cout << "YOLOv8 ONNXモデル検出（入力サイズ: " << input_size_ << "x" << input_size_ << "）" << endl;
        
    } else if (model_path.find(".weights") != string::npos) {
        // Darknet (YOLOv3/v4) モデル
        string cfg = model_path;
        cfg.replace(cfg.find(".weights"), 8, ".cfg");
        net_ = readNetFromDarknet(cfg, model_path);
        is_yolov8_ = false;
        input_size_ = 320;
        cout << "Darknet YOLOモデル検出" << endl;
        
    } else if (model_path.find(".tflite") != string::npos) {
        // TensorFlow Liteモデル
        net_ = readNetFromTensorflow(model_path);
        is_yolov8_ = false;
    } else if (model_path.find(".pb") != string::npos) {
        // TensorFlowモデル（frozen graph）
        net_ = readNetFromTensorflow(model_path);
        is_yolov8_ = false;
    } else if (model_path.find(".caffemodel") != string::npos) {
        // Caffeモデル
        string prototxt = model_path;
        prototxt.replace(prototxt.find(".caffemodel"), 12, ".prototxt");
        net_ = readNetFromCaffe(prototxt, model_path);
        is_yolov8_ = false;
    } else {
        net_ = readNetFromTensorflow(model_path);
        is_yolov8_ = false;
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
    
    // 入力画像の前処理
    Mat blob = blobFromImage(frame, 1/255.0, Size(input_size_, input_size_), Scalar(0, 0, 0), true, false);
    
    // モデルに入力
    net_.setInput(blob);
    
    // 推論を実行
    vector<Mat> outputs;
    net_.forward(outputs, net_.getUnconnectedOutLayersNames());
    
    if (is_yolov8_) {
        // YOLOv8の出力形式で解析
        detections = parseYOLOv8Output(outputs, frame.cols, frame.rows);
    } else {
        // YOLOv3/v4の出力形式で解析
        detections = parseYOLOv3v4Output(outputs, frame.cols, frame.rows);
    }
    
    return detections;
}

vector<DetectedObject> ObjectDetector::parseYOLOv3v4Output(const vector<Mat>& outputs, int frame_width, int frame_height) {
    vector<DetectedObject> detections;
    vector<int> class_ids;
    vector<float> confidences;
    vector<Rect> boxes;
    
    // YOLOv3/v4の結果を解析
    for (size_t i = 0; i < outputs.size(); ++i) {
        float* data = (float*)outputs[i].data;
        for (int j = 0; j < outputs[i].rows; ++j, data += outputs[i].cols) {
            Mat scores = outputs[i].row(j).colRange(5, outputs[i].cols);
            Point class_id_point;
            double confidence;
            minMaxLoc(scores, 0, &confidence, 0, &class_id_point);
            
            if (confidence > confidence_threshold_) {
                int center_x = (int)(data[0] * frame_width);
                int center_y = (int)(data[1] * frame_height);
                int width = (int)(data[2] * frame_width);
                int height = (int)(data[3] * frame_height);
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

vector<DetectedObject> ObjectDetector::parseYOLOv8Output(const vector<Mat>& outputs, int frame_width, int frame_height) {
    vector<DetectedObject> detections;
    vector<int> class_ids;
    vector<float> confidences;
    vector<Rect> boxes;
    
    // YOLOv8の出力は [1, 84, 8400] (COCO 80クラスの場合)
    // 形式: [x_center, y_center, width, height, class_scores...]
    if (outputs.empty()) {
        return detections;
    }
    
    Mat output = outputs[0];
    
    // 出力を転置して扱いやすくする: [8400, 84]
    if (output.dims == 3) {
        // [1, 84, 8400] -> [8400, 84]
        output = output.reshape(1, output.size[1]); // [84, 8400]
        cv::transpose(output, output);               // [8400, 84]
    }
    
    int num_classes = output.cols - 4; // 最初の4列は座標、残りがクラススコア
    float scale_x = (float)frame_width / input_size_;
    float scale_y = (float)frame_height / input_size_;
    
    // 各検出候補をループ
    for (int i = 0; i < output.rows; ++i) {
        // クラススコアの最大値を取得（5列目以降）
        Mat scores = output.row(i).colRange(4, output.cols);
        Point class_id_point;
        double max_class_score;
        minMaxLoc(scores, 0, &max_class_score, 0, &class_id_point);
        
        // 信頼度の閾値チェック
        if (max_class_score > confidence_threshold_) {
            // 座標取得（中心座標とサイズ）
            float cx = output.at<float>(i, 0) * scale_x;
            float cy = output.at<float>(i, 1) * scale_y;
            float w  = output.at<float>(i, 2) * scale_x;
            float h  = output.at<float>(i, 3) * scale_y;
            
            // 左上座標に変換
            int left = (int)(cx - w / 2);
            int top  = (int)(cy - h / 2);
            
            class_ids.push_back(class_id_point.x);
            confidences.push_back((float)max_class_score);
            boxes.push_back(Rect(left, top, (int)w, (int)h));
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
