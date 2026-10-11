#include "audio/AudioManager.hpp"
#include <iostream>

bool AudioManager::playMusic(const std::string& path, bool loop) {
    if (!m_music.openFromFile(path)) {
        std::cerr << "AudioManager Error: Cannot open music file " << path << std::endl;
        return false;
    }

    // SFML 3 API method for looping
    m_music.setLooping(loop);
    m_music.play();
    return true;
}

void AudioManager::stopMusic() {
    m_music.stop();
}

void AudioManager::setMusicVolume(float volume) {
    m_music.setVolume(volume);
}

bool AudioManager::playSound(const std::string& path) {
    // Cache sound buffer if not yet loaded
    auto it = m_soundBuffers.find(path);
    if (it == m_soundBuffers.end()) {
        sf::SoundBuffer buffer;
        if (!buffer.loadFromFile(path)) {
            std::cerr << "AudioManager Error: Cannot load sound effect " << path << std::endl;
            return false;
        }
        auto [insertedIt, success] = m_soundBuffers.emplace(path, std::move(buffer));
        it = insertedIt;
    }

    // SFML 3: Construct sf::Sound directly with the buffer reference
    m_activeSounds.emplace_back(it->second);
    m_activeSounds.back().play();
    return true;
}

void AudioManager::update() {
    // Remove completed sound instances from memory
    m_activeSounds.remove_if([](const sf::Sound& sound) {
        return sound.getStatus() == sf::Sound::Status::Stopped;
    });
}