/**
 * @file voice_detector.cpp
 * @brief Implementation of voice/sound detection
 * @author RobotC Project
 * @date 2026-01-23
 */

#include "audio/voice_detector.h"
#include <iostream>
#include <alsa/asoundlib.h>
#include <cmath>

VoiceDetector::VoiceDetector() : capture_handle_(nullptr), threshold_(0.05f) {}

VoiceDetector::~VoiceDetector() {
    close();
}

bool VoiceDetector::init(const std::string& device) {
    device_ = device;
    
    snd_pcm_t* handle;
    int err = snd_pcm_open(&handle, device.c_str(), SND_PCM_STREAM_CAPTURE, SND_PCM_NONBLOCK);
    if (err < 0) {
        std::cerr << "Failed to open audio device: " << snd_strerror(err) << std::endl;
        return false;
    }
    
    // パラメータ設定
    snd_pcm_hw_params_t* params;
    snd_pcm_hw_params_alloca(&params);
    snd_pcm_hw_params_any(handle, params);
    
    snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(handle, params, SND_PCM_FORMAT_S16_LE);
    
    unsigned int rate = 16000;
    snd_pcm_hw_params_set_rate_near(handle, params, &rate, 0);
    snd_pcm_hw_params_set_channels(handle, params, 1);
    
    err = snd_pcm_hw_params(handle, params);
    if (err < 0) {
        std::cerr << "Failed to set audio parameters: " << snd_strerror(err) << std::endl;
        snd_pcm_close(handle);
        return false;
    }
    
    snd_pcm_prepare(handle);
    capture_handle_ = handle;
    
    std::cout << "Voice detector initialized" << std::endl;
    return true;
}

void VoiceDetector::close() {
    if (capture_handle_) {
        snd_pcm_close((snd_pcm_t*)capture_handle_);
        capture_handle_ = nullptr;
    }
}

bool VoiceDetector::detectVoice() {
    if (!capture_handle_) return false;
    
    snd_pcm_t* handle = (snd_pcm_t*)capture_handle_;
    
    // 短時間サンプリング（100ms = 1600サンプル @ 16kHz）
    const int frames = 1600;
    int16_t buffer[frames];
    
    int err = snd_pcm_readi(handle, buffer, frames);
    if (err < 0) {
        if (err == -EAGAIN) {
            return false;  // データなし
        }
        snd_pcm_recover(handle, err, 0);
        return false;
    }
    
    // RMS計算
    float sum = 0.0f;
    for (int i = 0; i < err; i++) {
        float sample = buffer[i] / 32768.0f;
        sum += sample * sample;
    }
    float rms = std::sqrt(sum / err);
    
    // 閾値超過で音声検知
    return (rms > threshold_);
}
