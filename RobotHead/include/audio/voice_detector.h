/**
 * @file voice_detector.h
 * @brief Voice/sound detection using microphone input
 * @author RobotC Project
 * @date 2026-01-23
 */

#ifndef VOICE_DETECTOR_H
#define VOICE_DETECTOR_H

#include <string>

class VoiceDetector {
public:
    VoiceDetector();
    ~VoiceDetector();
    
    bool init(const std::string& device = "default");
    void close();
    
    bool detectVoice();  // 音声検知（音量レベルで判定）
    
private:
    void* capture_handle_;
    std::string device_;
    float threshold_;
};

#endif // VOICE_DETECTOR_H
