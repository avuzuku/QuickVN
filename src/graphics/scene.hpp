#pragma once
#include "audio/audio.hpp"
#include "rust_core.h"
#include "ui/ui.hpp"
#include <SFML/Graphics.hpp>
#include <cstdint>
#include <list>
#include <optional>
#include <string>
#include <vector>

struct CharacterSlot {
    std::string id;
    sf::Texture texture;
    std::optional<sf::Sprite> sprite;
    bool active = false;
    bool hiding = false;
    bool flip_x = false;

    sf::Texture prev_texture;
    std::optional<sf::Sprite> prev_sprite;
    bool has_prev = false;

    float fade_elapsed = 0.f;
    float fade_duration = 0.f;
    float start_alpha = 255.f;
    float target_alpha = 255.f;
    float alpha = 255.f;

    sf::Vector2f pos = {640.f, 720.f};
    sf::Vector2f start_pos = {640.f, 720.f};
    sf::Vector2f target_pos = {640.f, 720.f};
    float move_elapsed = 0.f;
    float move_duration = 0.f;

    float scale = 1.f;
    float start_scale = 1.f;
    float target_scale = 1.f;
    float scale_elapsed = 0.f;
    float scale_duration = 0.f;

    float bounce_timer = 0.f;
    float brightness = 255.f;
    float target_brightness = 255.f;
    float breath_phase = 0.f;
};

struct ChoiceButton {
    sf::FloatRect rect;
    std::string label;
    float hover_t = 0.f;
};

struct SceneParticle {
    sf::Vector2f pos;
    sf::Vector2f vel;
    float rotation = 0.f;
    float rot_speed = 0.f;
    float size = 4.f;
    float alpha = 200.f;
    sf::Color color = sf::Color::White;
};

struct SceneState {
    sf::Font font;
    std::vector<uint8_t> font_buffer;
    std::string loaded_font_path;
    std::optional<sf::Text> text_name;
    std::optional<sf::Text> text_dialogue;

    sf::Texture bg_texture;
    std::optional<sf::Sprite> bg_sprite;
    bool has_bg = false;

    sf::Texture prev_bg_texture;
    std::optional<sf::Sprite> prev_bg_sprite;
    bool has_prev_bg = false;
    float bg_fade_elapsed = 0.f;
    float bg_fade_duration = 0.f;
    float bg_alpha = 255.f;

    std::list<CharacterSlot> characters;
    std::string active_speaker_id;

    float cam_zoom = 1.f;
    float cam_start_zoom = 1.f;
    float cam_target_zoom = 1.f;
    float cam_zoom_elapsed = 0.f;
    float cam_zoom_duration = 0.f;

    sf::Vector2f cam_pan = {0.f, 0.f};
    sf::Vector2f cam_start_pan = {0.f, 0.f};
    sf::Vector2f cam_target_pan = {0.f, 0.f};
    float cam_pan_elapsed = 0.f;
    float cam_pan_duration = 0.f;

    float shake_intensity = 0.f;
    float shake_duration = 0.f;
    float shake_elapsed = 0.f;
    sf::Vector2f shake_offset = {0.f, 0.f};

    sf::Color flash_color = sf::Color::White;
    float flash_duration = 0.f;
    float flash_elapsed = 0.f;
    float flash_alpha = 0.f;

    sf::Color tint_start_color = sf::Color(0, 0, 0, 0);
    sf::Color tint_target_color = sf::Color(0, 0, 0, 0);
    sf::Color tint_current_color = sf::Color(0, 0, 0, 0);
    float tint_elapsed = 0.f;
    float tint_duration = 0.f;

    float vignette_alpha = 0.f;
    float cinema_t = 0.f;
    bool cinema_enabled = false;

    std::string weather_type = "none";
    std::vector<SceneParticle> particles;

    bool nvl_mode = false;
    std::vector<std::string> nvl_history;

    std::string speaker_name;
    sf::Color speaker_color = sf::Color(250, 204, 21);
    std::string raw_dialogue;
    std::string wrapped_dialogue;
    size_t total_chars = 0;
    size_t visible_chars = 0;
    float typewriter_elapsed = 0.f;
    float typewriter_cps = 48.f;
    bool typewriter_active = false;
    uint32_t sound_char_counter = 0;
    float cursor_blink_timer = 0.f;
    float breath_time = 0.f;

    std::vector<std::string> current_choice_labels;
    std::vector<ChoiceButton> choice_buttons;
    bool has_choices = false;
    float choice_timer_total = 0.f;
    float choice_timer_remaining = 0.f;

    std::string chapter_title;
    std::string chapter_subtitle;
    float chapter_timer = 0.f;
    float chapter_duration = 0.f;

    std::string toast_text;
    float toast_timer = 0.f;
    float toast_duration = 2.0f;

    std::string ach_toast_title;
    float ach_toast_timer = 0.f;
};

bool scene_init(SceneState& scene, const QvnConfig& cfg, const std::string& lang = "en");
void scene_apply_config(SceneState& scene, const QvnConfig& cfg, const std::string& lang = "en");
void scene_reset_visuals(SceneState& scene, const QvnConfig& cfg);

bool scene_set_background(SceneState& scene, const QvnConfig& cfg, const std::string& path, float fade_duration);
bool scene_set_character(
    SceneState& scene,
    const QvnConfig& cfg,
    const std::string& id,
    const std::string& path,
    float x,
    float y,
    float fade_duration,
    float move_duration
);
void scene_move_character(SceneState& scene, const QvnConfig& cfg, const std::string& id, float x, float y, float move_duration);
void scene_hide_character(SceneState& scene, const QvnConfig& cfg, const std::string& id, float fade_duration);
void scene_hide_all_characters(SceneState& scene, const QvnConfig& cfg, float fade_duration);
void scene_scale_character(SceneState& scene, const std::string& id, float scale, float duration);
void scene_bounce_character(SceneState& scene, const std::string& id);
void scene_flip_character(SceneState& scene, const std::string& id);

void scene_trigger_cam_zoom(SceneState& scene, float zoom, float duration);
void scene_trigger_cam_pan(SceneState& scene, float x, float y, float duration);
void scene_trigger_shake(SceneState& scene, float intensity, float duration);
void scene_trigger_flash(SceneState& scene, const std::string& hex_color, float duration);
void scene_trigger_tint(SceneState& scene, const std::string& hex_color, float duration);
void scene_set_vignette(SceneState& scene, float alpha);
void scene_set_cinema(SceneState& scene, bool enabled);
void scene_set_weather(SceneState& scene, const std::string& type, float density);

void scene_show_chapter(SceneState& scene, const std::string& title, const std::string& subtitle, float duration);
void scene_show_toast(SceneState& scene, const std::string& text, float duration = 2.2f);
void scene_show_achievement_toast(SceneState& scene, const std::string& title, float duration = 3.5f);

void scene_set_nvl_mode(SceneState& scene, bool enabled);
void scene_clear_nvl(SceneState& scene);

void scene_set_dialogue(
    SceneState& scene,
    const QvnConfig& cfg,
    const std::string& name,
    const std::string& text,
    float cps_override,
    const std::string& color_hex,
    const std::string& speaker_id
);
bool scene_is_typing(const SceneState& scene);
void scene_skip_typewriter(SceneState& scene);

void scene_set_choices(SceneState& scene, const QvnConfig& cfg, const std::vector<std::string>& choices, float timer_sec = 0.f);
void scene_update_choice_hover(SceneState& scene, const sf::Vector2f& mouse_pos);
int scene_get_choice_at(const SceneState& scene, const sf::Vector2f& mouse_pos);
void scene_clear_choices(SceneState& scene);

void scene_update(SceneState& scene, const QvnConfig& cfg, AudioState& audio, float dt);
void scene_render(const SceneState& scene, const QvnConfig& cfg, UiContext& ui, sf::RenderWindow& window, bool show_dialogue);