#pragma once
#include "rust_core.h"
#include <SFML/Audio.hpp>
#include <cstdint>
#include <list>
#include <string>
#include <unordered_map>
#include <vector>

struct AudioState {
    sf::Music music;
    std::vector<uint8_t> music_buffer;
    std::string current_music_path;
    bool music_looping = true;
    bool music_playing = false;

    sf::Music voice;
    std::vector<uint8_t> voice_buffer;

    float music_volume = 75.f;
    float sound_volume = 100.f;
    float voice_volume = 100.f;

    float fade_cur = 1.f;
    float fade_target = 1.f;
    float fade_speed = 0.f;
    bool stop_after_fade = false;

    std::unordered_map<std::string, sf::SoundBuffer> sound_buffers;
    std::list<sf::Sound> active_sounds;
};

void audio_init(AudioState& state, const QvnConfig& cfg);
void audio_apply_config(AudioState& state, const QvnConfig& cfg);
bool audio_play_music(AudioState& state, const std::string& path, bool loop = true, float fade_sec = 0.35f);
void audio_stop_music(AudioState& state, float fade_sec = 0.35f);
void audio_set_music_volume(AudioState& state, float volume);
void audio_set_sound_volume(AudioState& state, float volume);
void audio_set_voice_volume(AudioState& state, float volume);
bool audio_play_sound(AudioState& state, const std::string& path);
bool audio_play_voice(AudioState& state, const std::string& path);
void audio_stop_voice(AudioState& state);
void audio_update(AudioState& state, float dt);
void audio_shutdown(AudioState& state);
