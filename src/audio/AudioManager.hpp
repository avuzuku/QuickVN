#pragma once
#include <SFML/Audio.hpp>
#include <string>
#include <unordered_map>
#include <list>

class AudioManager {
public:
    AudioManager() = default;
    ~AudioManager() = default;

    bool playMusic(const std::string& path, bool loop = true);
    void stopMusic();
    void setMusicVolume(float volume);

    bool playSound(const std::string& path);
    void update(); // Cleans up finished sounds

private:
    sf::Music m_music;
    std::unordered_map<std::string, sf::SoundBuffer> m_soundBuffers;
    std::list<sf::Sound> m_activeSounds;
};