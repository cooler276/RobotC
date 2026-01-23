/**
 * @file audio_player.h
 * @brief Audio playback manager for robot voice output
 * @author RobotC Project
 * @date 2026-01-23
 */

#ifndef AUDIO_PLAYER_H
#define AUDIO_PLAYER_H

#include <string>
#include <vector>

/**
 * @class AudioPlayer
 * @brief Manages audio playback for robot voice interactions
 * 
 * This class provides functionality to play audio files using ALSA,
 * including greeting messages and response sounds.
 */
class AudioPlayer {
public:
    /** @brief Constructor */
    AudioPlayer();
    
    /** @brief Destructor */
    ~AudioPlayer();
    
    /**
     * @brief Initialize audio player
     * @param device ALSA device name (default: "default")
     * @return true if initialization successful, false otherwise
     */
    bool init(const std::string& device = "default");
    
    /**
     * @brief Close audio player and release resources
     */
    void close();
    
    /**
     * @brief Play a specific audio file
     * @param filename Path to audio file
     * @return true if playback started successfully, false otherwise
     */
    bool playFile(const std::string& filename);
    
    /**
     * @brief Play a random greeting message
     * @return true if playback started successfully, false otherwise
     */
    bool playRandomGreeting();
    
    /**
     * @brief Play a random response sound
     * @return true if playback started successfully, false otherwise
     */
    bool playRandomResponse();
    
    /**
     * @brief Check if audio is currently playing
     * @return true if playing, false otherwise
     */
    bool isPlaying() const { return is_playing_; }
    
private:
    /**
     * @brief Play a random file from a list
     * @param files Vector of file paths
     * @return true if playback started successfully, false otherwise
     */
    bool playRandomFromList(const std::vector<std::string>& files);
    
    bool is_playing_;                              ///< Playback status flag
    std::string device_;                           ///< ALSA device name
    std::vector<std::string> greeting_files_;      ///< List of greeting audio files
    std::vector<std::string> response_files_;      ///< List of response audio files
};

#endif // AUDIO_PLAYER_H
