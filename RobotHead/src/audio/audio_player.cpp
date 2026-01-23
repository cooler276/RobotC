/**
 * @file audio_player.cpp
 * @brief Implementation of audio playback manager
 * @author RobotC Project
 * @date 2026-01-23
 */

#include "audio/audio_player.h"
#include <iostream>
#include <cstdlib>
#include <random>
#include <chrono>

AudioPlayer::AudioPlayer() : is_playing_(false) {
    std::cout << "[AudioPlayer] Initializing audio player..." << std::endl;
    
    // 挨拶音声ファイルリスト（5個）
    for (int i = 1; i <= 5; i++) {
        greeting_files_.push_back("/home/ryo/work/voice/greeting" + std::to_string(i) + ".wav");
    }
    std::cout << "[AudioPlayer] Loaded " << greeting_files_.size() << " greeting files" << std::endl;
    
    // 相槌音声ファイルリスト（5個）
    for (int i = 1; i <= 5; i++) {
        response_files_.push_back("/home/ryo/work/voice/response" + std::to_string(i) + ".wav");
    }
    std::cout << "[AudioPlayer] Loaded " << response_files_.size() << " response files" << std::endl;
}

AudioPlayer::~AudioPlayer() {
    close();
}

bool AudioPlayer::init(const std::string& device) {
    device_ = device;
    return true;
}

void AudioPlayer::close() {
    is_playing_ = false;
}

bool AudioPlayer::playFile(const std::string& filename) {
    std::cout << "[AudioPlayer] Attempting to play: " << filename << std::endl;
    
    // ファイル存在確認
    std::string check_cmd = "test -f " + filename;
    int check_ret = system(check_cmd.c_str());
    if (check_ret != 0) {
        std::cerr << "[AudioPlayer] ERROR: File not found: " << filename << std::endl;
        return false;
    }
    std::cout << "[AudioPlayer] File exists, proceeding to play" << std::endl;
    
    // aplayコマンドを使用して非同期再生
    std::string cmd = "aplay -q " + filename + " &";
    std::cout << "[AudioPlayer] Executing command: " << cmd << std::endl;
    int ret = system(cmd.c_str());
    
    if (ret == 0) {
        is_playing_ = true;
        std::cout << "[AudioPlayer] ✓ Audio playback started successfully" << std::endl;
        return true;
    } else {
        std::cerr << "[AudioPlayer] ERROR: system() returned " << ret << " for: " << filename << std::endl;
        return false;
    }
}

bool AudioPlayer::playRandomFromList(const std::vector<std::string>& files) {
    std::cout << "[AudioPlayer] playRandomFromList called" << std::endl;
    
    if (files.empty()) {
        std::cerr << "[AudioPlayer] ERROR: No audio files in list" << std::endl;
        return false;
    }
    
    std::cout << "[AudioPlayer] List contains " << files.size() << " files" << std::endl;
    
    // ランダム選択
    static std::mt19937 rng(std::chrono::steady_clock::now().time_since_epoch().count());
    std::uniform_int_distribution<> dist(0, files.size() - 1);
    
    int index = dist(rng);
    std::cout << "[AudioPlayer] Selected index " << index << " from " << files.size() << " files" << std::endl;
    std::cout << "[AudioPlayer] Selected file: " << files[index] << std::endl;
    
    return playFile(files[index]);
}

bool AudioPlayer::playRandomGreeting() {
    std::cout << "[AudioPlayer] ========== PLAY RANDOM GREETING ==========" << std::endl;
    bool result = playRandomFromList(greeting_files_);
    std::cout << "[AudioPlayer] Greeting playback result: " << (result ? "SUCCESS" : "FAILED") << std::endl;
    return result;
}

bool AudioPlayer::playRandomResponse() {
    std::cout << "[AudioPlayer] ========== PLAY RANDOM RESPONSE ==========" << std::endl;
    bool result = playRandomFromList(response_files_);
    std::cout << "[AudioPlayer] Response playback result: " << (result ? "SUCCESS" : "FAILED") << std::endl;
    return result;
}
