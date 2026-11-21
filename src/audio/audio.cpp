#include "audio/audio.hpp"
#include "platform/platform.hpp"
#include <algorithm>
#include <iostream>

void audio_init(AudioState& state, const QvnConfig& cfg) {
    audio_apply_config(state, cfg);
}

void audio_apply_config(AudioState& state, const QvnConfig& cfg) {
    state.music_volume = cfg.music_volume;
    state.sound_volume = cfg.sound_volume;
    state.voice_volume = cfg.voice_volume;
    state.music.setVolume(state.music_volume * state.fade_cur);
    state.voice.setVolume(state.voice_volume);
    for (auto& snd : state.active_sounds) {
        snd.setVolume(state.sound_volume);
    }
}

bool audio_play_music(AudioState& state, const std::string& path, bool loop, float fade_sec) {
    if (path.empty()) return false;
    if (state.music_playing && state.current_music_path == path && state.music_looping == loop && !state.stop_after_fade) {
        return true;
    }
    state.music.stop();
    std::vector<uint8_t> raw;
    if (!platform_read_bytes_smart(path, "music", raw) || raw.empty()) {
        std::cerr << "Audio Error: Cannot read music file " << path << std::endl;
        return false;
    }
    state.music_buffer = std::move(raw);
    if (!state.music.openFromMemory(state.music_buffer.data(), state.music_buffer.size())) {
        std::cerr << "Audio Error: Cannot decode music file " << path << std::endl;
        state.music_buffer.clear();
        return false;
    }
    state.music.setLooping(loop);
    state.stop_after_fade = false;
    if (fade_sec > 0.01f) {
        state.fade_cur = 0.f;
        state.fade_target = 1.f;
        state.fade_speed = 1.f / fade_sec;
    } else {
        state.fade_cur = 1.f;
        state.fade_target = 1.f;
        state.fade_speed = 0.f;
    }
    state.music.setVolume(state.music_volume * state.fade_cur);
    state.music.play();
    state.current_music_path = path;
    state.music_looping = loop;
    state.music_playing = true;
    return true;
}

void audio_stop_music(AudioState& state, float fade_sec) {
    if (!state.music_playing) return;
    if (fade_sec > 0.01f) {
        state.fade_target = 0.f;
        state.fade_speed = 1.f / fade_sec;
        state.stop_after_fade = true;
    } else {
        state.music.stop();
        state.music_buffer.clear();
        state.current_music_path.clear();
        state.music_playing = false;
        state.stop_after_fade = false;
    }
}

void audio_set_music_volume(AudioState& state, float volume) {
    state.music_volume = volume;
    state.music.setVolume(volume * state.fade_cur);
}

void audio_set_sound_volume(AudioState& state, float volume) {
    state.sound_volume = volume;
    for (auto& snd : state.active_sounds) snd.setVolume(volume);
}

void audio_set_voice_volume(AudioState& state, float volume) {
    state.voice_volume = volume;
    state.voice.setVolume(volume);
}

bool audio_play_voice(AudioState& state, const std::string& path) {
    if (path.empty()) return false;
    state.voice.stop();
    std::vector<uint8_t> raw;
    if (!platform_read_bytes_smart(path, "voice", raw) || raw.empty()) return false;
    state.voice_buffer = std::move(raw);
    if (!state.voice.openFromMemory(state.voice_buffer.data(), state.voice_buffer.size())) {
        state.voice_buffer.clear();
        return false;
    }
    state.voice.setLooping(false);
    state.voice.setVolume(state.voice_volume);
    state.voice.play();
    return true;
}

void audio_stop_voice(AudioState& state) {
    state.voice.stop();
    state.voice_buffer.clear();
}

bool audio_play_sound(AudioState& state, const std::string& path) {
    if (path.empty()) return false;
    auto it = state.sound_buffers.find(path);
    if (it == state.sound_buffers.end()) {
        std::vector<uint8_t> raw;
        if (!platform_read_bytes_smart(path, "sfx", raw) || raw.empty()) {
            std::cerr << "Audio Error: Cannot read sound effect " << path << std::endl;
            return false;
        }
        sf::SoundBuffer buffer;
        if (!buffer.loadFromMemory(raw.data(), raw.size())) {
            std::cerr << "Audio Error: Cannot decode sound effect " << path << std::endl;
            return false;
        }
        auto [inserted_it, ok] = state.sound_buffers.emplace(path, std::move(buffer));
        it = inserted_it;
    }

    state.active_sounds.emplace_back(it->second);
    state.active_sounds.back().setVolume(state.sound_volume);
    state.active_sounds.back().play();
    return true;
}

void audio_update(AudioState& state, float dt) {
    state.active_sounds.remove_if([](const sf::Sound& sound) {
        return sound.getStatus() == sf::Sound::Status::Stopped;
    });

    if (state.music_playing && state.fade_speed > 0.f) {
        if (state.fade_cur < state.fade_target) {
            state.fade_cur = std::min(state.fade_target, state.fade_cur + state.fade_speed * dt);
        } else if (state.fade_cur > state.fade_target) {
            state.fade_cur = std::max(state.fade_target, state.fade_cur - state.fade_speed * dt);
        }
        state.music.setVolume(state.music_volume * state.fade_cur);
        if (state.stop_after_fade && state.fade_cur <= 0.001f) {
            state.music.stop();
            state.music_buffer.clear();
            state.current_music_path.clear();
            state.music_playing = false;
            state.stop_after_fade = false;
            state.fade_speed = 0.f;
        } else if (std::abs(state.fade_cur - state.fade_target) < 0.001f) {
            state.fade_speed = 0.f;
        }
    }
}

void audio_shutdown(AudioState& state) {
    state.music.stop();
    state.music_buffer.clear();
    state.voice.stop();
    state.voice_buffer.clear();
    state.active_sounds.clear();
    state.sound_buffers.clear();
    state.current_music_path.clear();
    state.music_playing = false;
}