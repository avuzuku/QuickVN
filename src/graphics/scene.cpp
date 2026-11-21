#include "graphics/scene.hpp"
#include "platform/platform.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <sstream>

static constexpr float LOGICAL_W = 1280.f;
static constexpr float LOGICAL_H = 720.f;

static sf::Color to_sf_color(QvnColor c) {
    return sf::Color(c.r, c.g, c.b, c.a);
}

static sf::Color parse_hex_color(const std::string& hex, sf::Color fallback) {
    if (hex.empty()) return fallback;
    std::string s = hex[0] == '#' ? hex.substr(1) : hex;
    if (s.size() != 6 && s.size() != 8) return fallback;
    unsigned int val = 0;
    try {
        val = static_cast<unsigned int>(std::stoul(s, nullptr, 16));
    } catch (...) {
        return fallback;
    }
    if (s.size() == 6) {
        return sf::Color(
            static_cast<uint8_t>((val >> 16) & 0xFF),
            static_cast<uint8_t>((val >> 8) & 0xFF),
            static_cast<uint8_t>(val & 0xFF),
            255
        );
    }
    return sf::Color(
        static_cast<uint8_t>((val >> 24) & 0xFF),
        static_cast<uint8_t>((val >> 16) & 0xFF),
        static_cast<uint8_t>((val >> 8) & 0xFF),
        static_cast<uint8_t>(val & 0xFF)
    );
}

static uint8_t clamp_u8(float v) {
    if (v <= 0.f) return 0;
    if (v >= 255.f) return 255;
    return static_cast<uint8_t>(v);
}

static bool load_texture_smart(sf::Texture& tex, const std::string& path, const char* kind) {
    std::vector<uint8_t> raw;
    if (!platform_read_bytes_smart(path, kind, raw) || raw.empty()) {
        if (!platform_read_bytes_smart(path, "image", raw) || raw.empty()) return false;
    }
    return tex.loadFromMemory(raw.data(), raw.size());
}

static std::string select_font_for_lang(const QvnConfig& cfg, const std::string& lang) {
    if (lang == "ja" && cfg.font_ja_path[0] != '\0') return cfg.font_ja_path;
    if (lang == "ru" && cfg.font_ru_path[0] != '\0') return cfg.font_ru_path;
    return cfg.font_path;
}

static bool load_font_vfs(SceneState& scene, const std::string& path) {
    std::vector<uint8_t> raw;
    if (!path.empty()) {
        std::string resolved = platform_resolve_asset_path(path);
        platform_read_bytes(resolved, raw);
    }
    if (raw.empty()) {
        size_t blen = 0;
        const uint8_t* bptr = qvn_builtin_font_data(&blen);
        if (bptr && blen > 0) raw.assign(bptr, bptr + blen);
    }
    if (raw.empty()) return false;
    scene.font_buffer = std::move(raw);
    if (!scene.font.openFromMemory(scene.font_buffer.data(), scene.font_buffer.size())) return false;
    scene.loaded_font_path = path;
    return true;
}

static void apply_bg_scale(sf::Sprite& sprite, const sf::Texture& tex) {
    sf::Vector2u size = tex.getSize();
    if (size.x > 0 && size.y > 0) {
        sprite.setScale({LOGICAL_W / static_cast<float>(size.x), LOGICAL_H / static_cast<float>(size.y)});
    }
}

static void apply_char_origin(sf::Sprite& sprite, const sf::Texture& tex) {
    sf::Vector2u size = tex.getSize();
    sprite.setOrigin({static_cast<float>(size.x) * 0.5f, static_cast<float>(size.y)});
}

static std::string wrap_dialogue_text(const sf::Font& font, uint32_t font_size, float max_width, const std::string& text) {
    if (max_width <= 0.f || text.empty()) return text;
    sf::Text measure(font, "", font_size);
    std::string result;
    std::istringstream line_stream(text);
    std::string raw_line;
    bool first_line = true;

    while (std::getline(line_stream, raw_line, '\n')) {
        if (!first_line) result.push_back('\n');
        first_line = false;

        std::string current_line;
        size_t i = 0;
        while (i < raw_line.size()) {
            unsigned char lead = static_cast<unsigned char>(raw_line[i]);
            size_t tok_end = i;
            if (lead < 0x80 && raw_line[i] == ' ') {
                tok_end = i + 1;
            } else if (lead >= 0xE0) {
                tok_end = std::min(raw_line.size(), i + ((lead >= 0xF0) ? 4 : 3));
            } else {
                while (tok_end < raw_line.size()) {
                    unsigned char c = static_cast<unsigned char>(raw_line[tok_end]);
                    if (c == ' ' || c >= 0xE0) break;
                    tok_end++;
                }
            }
            std::string token = raw_line.substr(i, tok_end - i);
            i = tok_end;
            if (current_line.empty() && token == " ") continue;

            std::string cand = current_line + token;
            measure.setString(sf::String::fromUtf8(cand.begin(), cand.end()));
            if (!current_line.empty() && measure.getLocalBounds().size.x > max_width) {
                while (!current_line.empty() && current_line.back() == ' ') current_line.pop_back();
                result += current_line;
                result.push_back('\n');
                current_line = (token == " ") ? "" : token;
            } else {
                current_line = std::move(cand);
            }
        }
        result += current_line;
    }
    return result;
}

static CharacterSlot* find_char_slot(SceneState& scene, const std::string& id) {
    for (auto& c : scene.characters) {
        if (c.id == id) return &c;
    }
    return nullptr;
}

static float resolved_name_y(const QvnConfig& cfg) {
    return std::max(cfg.name_y, cfg.box_y + 14.f);
}

static void rebuild_choice_buttons(SceneState& scene, const QvnConfig& cfg) {
    scene.choice_buttons.clear();
    scene.has_choices = !scene.current_choice_labels.empty();
    if (!scene.has_choices) return;

    const float btn_width = cfg.choice_width;
    const float btn_height = cfg.choice_height;
    const float spacing = cfg.choice_spacing;
    const float count = static_cast<float>(scene.current_choice_labels.size());
    const float total_height = count * btn_height + (count - 1.f) * spacing;
    const float start_y = cfg.choice_center_y - (total_height * 0.5f);
    const float start_x = (LOGICAL_W - btn_width) * 0.5f;

    for (size_t i = 0; i < scene.current_choice_labels.size(); ++i) {
        ChoiceButton btn;
        float y = start_y + static_cast<float>(i) * (btn_height + spacing);
        btn.rect = sf::FloatRect({start_x, y}, {btn_width, btn_height});
        btn.label = scene.current_choice_labels[i];
        btn.hover_t = 0.f;
        scene.choice_buttons.push_back(std::move(btn));
    }
}

bool scene_init(SceneState& scene, const QvnConfig& cfg, const std::string& lang) {
    std::string target = select_font_for_lang(cfg, lang);
    if (!load_font_vfs(scene, target)) {
        if (!load_font_vfs(scene, cfg.font_path)) {
            std::cerr << "Scene Error: Failed to open font " << target << std::endl;
            return false;
        }
    }
    scene_apply_config(scene, cfg, lang);
    return true;
}

void scene_apply_config(SceneState& scene, const QvnConfig& cfg, const std::string& lang) {
    std::string target = select_font_for_lang(cfg, lang);
    if (scene.loaded_font_path != target) {
        load_font_vfs(scene, target);
    }

    float ny = resolved_name_y(cfg);
    if (!scene.text_name) {
        scene.text_name.emplace(scene.font, "", cfg.name_font_size);
    } else {
        scene.text_name->setCharacterSize(cfg.name_font_size);
    }
    scene.text_name->setFillColor(to_sf_color(cfg.name_color));
    scene.text_name->setPosition({cfg.name_x + 14.f, ny});

    if (!scene.text_dialogue) {
        scene.text_dialogue.emplace(scene.font, "", cfg.text_font_size);
    } else {
        scene.text_dialogue->setCharacterSize(cfg.text_font_size);
    }
    scene.text_dialogue->setLineSpacing(cfg.dialogue_line_spacing > 0.5f ? cfg.dialogue_line_spacing : 1.38f);
    scene.text_dialogue->setFillColor(to_sf_color(cfg.text_color));
    scene.text_dialogue->setPosition({cfg.text_x, std::max(cfg.text_y, ny + 34.f)});

    if (scene.has_bg && scene.bg_sprite) apply_bg_scale(*scene.bg_sprite, scene.bg_texture);
    if (scene.has_prev_bg && scene.prev_bg_sprite) apply_bg_scale(*scene.prev_bg_sprite, scene.prev_bg_texture);

    if (!scene.raw_dialogue.empty()) {
        float wrap_w = scene.nvl_mode ? (cfg.nvl_max_width > 100.f ? cfg.nvl_max_width : 840.f) : cfg.text_max_width;
        scene.wrapped_dialogue = wrap_dialogue_text(scene.font, cfg.text_font_size, wrap_w, scene.raw_dialogue);
        scene.total_chars = qvn_utf8_char_count(scene.wrapped_dialogue.c_str());
        if (!scene.typewriter_active) {
            scene.visible_chars = scene.total_chars;
            scene.text_dialogue->setString(sf::String::fromUtf8(scene.wrapped_dialogue.begin(), scene.wrapped_dialogue.end()));
        }
    }
    if (scene.has_choices) rebuild_choice_buttons(scene, cfg);
}

void scene_reset_visuals(SceneState& scene, const QvnConfig& cfg) {
    (void)cfg;
    scene.has_bg = false;
    scene.has_prev_bg = false;
    scene.bg_sprite.reset();
    scene.prev_bg_sprite.reset();
    scene.bg_alpha = 255.f;
    scene.bg_fade_elapsed = 0.f;
    scene.bg_fade_duration = 0.f;
    scene.characters.clear();
    scene.active_speaker_id.clear();
    scene.cam_zoom = 1.f;
    scene.cam_target_zoom = 1.f;
    scene.cam_zoom_duration = 0.f;
    scene.cam_pan = {0.f, 0.f};
    scene.cam_target_pan = {0.f, 0.f};
    scene.cam_pan_duration = 0.f;
    scene.shake_duration = 0.f;
    scene.shake_offset = {0.f, 0.f};
    scene.flash_duration = 0.f;
    scene.flash_alpha = 0.f;
    scene.tint_current_color = sf::Color(0, 0, 0, 0);
    scene.tint_target_color = sf::Color(0, 0, 0, 0);
    scene.tint_duration = 0.f;
    scene.cinema_enabled = false;
    scene.cinema_t = 0.f;
    scene.weather_type = "none";
    scene.particles.clear();
    scene.chapter_timer = 0.f;
    scene.nvl_mode = false;
    scene.nvl_history.clear();
    scene_clear_choices(scene);
}

bool scene_set_background(SceneState& scene, const QvnConfig& cfg, const std::string& path, float fade_duration) {
    float dur = fade_duration < 0.f ? cfg.bg_fade_duration : fade_duration;
    bool had_bg = scene.has_bg;

    if (had_bg && dur > 0.001f) {
        std::swap(scene.bg_texture, scene.prev_bg_texture);
        scene.prev_bg_sprite.emplace(scene.prev_bg_texture);
        apply_bg_scale(*scene.prev_bg_sprite, scene.prev_bg_texture);
        scene.prev_bg_sprite->setColor(sf::Color(255, 255, 255, 255));
        scene.has_prev_bg = true;
    } else {
        scene.has_prev_bg = false;
        scene.prev_bg_sprite.reset();
    }

    if (!load_texture_smart(scene.bg_texture, path, "bg")) {
        std::cerr << "Scene Error: Failed to load background " << path << std::endl;
        if (had_bg && dur > 0.001f) {
            std::swap(scene.bg_texture, scene.prev_bg_texture);
            scene.bg_sprite.emplace(scene.bg_texture);
            apply_bg_scale(*scene.bg_sprite, scene.bg_texture);
            scene.has_prev_bg = false;
            scene.prev_bg_sprite.reset();
        }
        return false;
    }

    scene.bg_sprite.emplace(scene.bg_texture);
    apply_bg_scale(*scene.bg_sprite, scene.bg_texture);
    scene.has_bg = true;

    scene.bg_fade_elapsed = 0.f;
    scene.bg_fade_duration = dur;
    if (dur > 0.001f) {
        scene.bg_alpha = 0.f;
    } else {
        scene.bg_alpha = 255.f;
        scene.has_prev_bg = false;
        scene.prev_bg_sprite.reset();
    }
    scene.bg_sprite->setColor(sf::Color(255, 255, 255, clamp_u8(scene.bg_alpha)));
    return true;
}

bool scene_set_character(
    SceneState& scene,
    const QvnConfig& cfg,
    const std::string& id,
    const std::string& path,
    float x,
    float y,
    float fade_duration,
    float move_duration
) {
    std::string slot_id = id.empty() ? "default" : id;
    CharacterSlot* slot = find_char_slot(scene, slot_id);
    if (!slot) {
        scene.characters.emplace_back();
        slot = &scene.characters.back();
        slot->id = slot_id;
        slot->pos = {cfg.default_char_x, cfg.default_char_y};
        slot->start_pos = slot->pos;
        slot->target_pos = slot->pos;
        slot->breath_phase = static_cast<float>(scene.characters.size()) * 1.3f;
    }

    float fdur = fade_duration < 0.f ? cfg.char_fade_duration : fade_duration;
    float mdur = move_duration < 0.f ? cfg.char_move_duration : move_duration;
    bool was_visible = slot->active && !slot->hiding;

    if (was_visible && fdur > 0.001f) {
        std::swap(slot->texture, slot->prev_texture);
        slot->prev_sprite.emplace(slot->prev_texture);
        apply_char_origin(*slot->prev_sprite, slot->prev_texture);
        slot->prev_sprite->setPosition(slot->pos);
        slot->prev_sprite->setScale({slot->flip_x ? -slot->scale : slot->scale, slot->scale});
        slot->prev_sprite->setColor(sf::Color(255, 255, 255, clamp_u8(slot->alpha)));
        slot->has_prev = true;
    } else {
        slot->has_prev = false;
        slot->prev_sprite.reset();
    }

    if (!load_texture_smart(slot->texture, path, "char")) {
        std::cerr << "Scene Error: Failed to load character " << path << std::endl;
        if (was_visible && fdur > 0.001f) {
            std::swap(slot->texture, slot->prev_texture);
            slot->sprite.emplace(slot->texture);
            apply_char_origin(*slot->sprite, slot->texture);
            slot->sprite->setPosition(slot->pos);
            slot->has_prev = false;
            slot->prev_sprite.reset();
        }
        return false;
    }

    float target_x = std::isnan(x) ? (was_visible ? slot->target_pos.x : cfg.default_char_x) : x;
    float target_y = std::isnan(y) ? (was_visible ? slot->target_pos.y : cfg.default_char_y) : y;

    if (was_visible && mdur > 0.001f) {
        slot->start_pos = slot->pos;
        slot->target_pos = {target_x, target_y};
        slot->move_elapsed = 0.f;
        slot->move_duration = mdur;
    } else {
        slot->pos = {target_x, target_y};
        slot->start_pos = slot->pos;
        slot->target_pos = slot->pos;
        slot->move_elapsed = 0.f;
        slot->move_duration = 0.f;
    }

    slot->sprite.emplace(slot->texture);
    apply_char_origin(*slot->sprite, slot->texture);
    slot->sprite->setPosition(slot->pos);
    slot->sprite->setScale({slot->flip_x ? -slot->scale : slot->scale, slot->scale});
    slot->active = true;
    slot->hiding = false;

    slot->fade_elapsed = 0.f;
    slot->fade_duration = fdur;
    slot->target_alpha = 255.f;
    if (fdur > 0.001f) {
        slot->start_alpha = 0.f;
        slot->alpha = 0.f;
    } else {
        slot->start_alpha = 255.f;
        slot->alpha = 255.f;
        slot->has_prev = false;
        slot->prev_sprite.reset();
    }
    slot->sprite->setColor(sf::Color(255, 255, 255, clamp_u8(slot->alpha)));
    return true;
}

void scene_move_character(SceneState& scene, const QvnConfig& cfg, const std::string& id, float x, float y, float move_duration) {
    CharacterSlot* slot = find_char_slot(scene, id.empty() ? "default" : id);
    if (!slot || !slot->active) return;

    float dur = move_duration < 0.f ? cfg.char_move_duration : move_duration;
    float target_x = std::isnan(x) ? slot->pos.x : x;
    float target_y = std::isnan(y) ? slot->pos.y : y;

    slot->start_pos = slot->pos;
    slot->target_pos = {target_x, target_y};
    slot->move_elapsed = 0.f;
    slot->move_duration = dur;

    if (dur <= 0.001f) {
        slot->pos = slot->target_pos;
        if (slot->sprite) slot->sprite->setPosition(slot->pos);
    }
}

void scene_hide_character(SceneState& scene, const QvnConfig& cfg, const std::string& id, float fade_duration) {
    CharacterSlot* slot = find_char_slot(scene, id.empty() ? "default" : id);
    if (!slot || !slot->active) return;

    float dur = fade_duration < 0.f ? cfg.char_fade_duration : fade_duration;
    slot->has_prev = false;
    slot->prev_sprite.reset();

    if (dur <= 0.001f) {
        slot->active = false;
        slot->sprite.reset();
        slot->hiding = false;
        slot->alpha = 0.f;
        return;
    }
    slot->hiding = true;
    slot->start_alpha = slot->alpha;
    slot->target_alpha = 0.f;
    slot->fade_elapsed = 0.f;
    slot->fade_duration = dur;
}

void scene_hide_all_characters(SceneState& scene, const QvnConfig& cfg, float fade_duration) {
    for (auto& c : scene.characters) {
        if (c.active) scene_hide_character(scene, cfg, c.id, fade_duration);
    }
}

void scene_scale_character(SceneState& scene, const std::string& id, float scale, float duration) {
    CharacterSlot* slot = find_char_slot(scene, id.empty() ? "default" : id);
    if (!slot || !slot->active) return;
    slot->start_scale = slot->scale;
    slot->target_scale = std::clamp(scale, 0.2f, 3.0f);
    slot->scale_elapsed = 0.f;
    slot->scale_duration = std::max(0.f, duration);
    if (slot->scale_duration <= 0.001f) {
        slot->scale = slot->target_scale;
        slot->scale_duration = 0.f;
        if (slot->sprite) slot->sprite->setScale({slot->flip_x ? -slot->scale : slot->scale, slot->scale});
    }
}

void scene_bounce_character(SceneState& scene, const std::string& id) {
    CharacterSlot* slot = find_char_slot(scene, id.empty() ? "default" : id);
    if (!slot || !slot->active) return;
    slot->bounce_timer = 0.36f;
}

void scene_flip_character(SceneState& scene, const std::string& id) {
    CharacterSlot* slot = find_char_slot(scene, id.empty() ? "default" : id);
    if (!slot || !slot->active) return;
    slot->flip_x = !slot->flip_x;
    if (slot->sprite) slot->sprite->setScale({slot->flip_x ? -slot->scale : slot->scale, slot->scale});
}

void scene_trigger_cam_zoom(SceneState& scene, float zoom, float duration) {
    scene.cam_start_zoom = scene.cam_zoom;
    scene.cam_target_zoom = std::clamp(zoom, 0.5f, 2.5f);
    scene.cam_zoom_elapsed = 0.f;
    scene.cam_zoom_duration = std::max(0.f, duration);
    if (scene.cam_zoom_duration <= 0.001f) {
        scene.cam_zoom = scene.cam_target_zoom;
        scene.cam_zoom_duration = 0.f;
    }
}

void scene_trigger_cam_pan(SceneState& scene, float x, float y, float duration) {
    scene.cam_start_pan = scene.cam_pan;
    scene.cam_target_pan = {x, y};
    scene.cam_pan_elapsed = 0.f;
    scene.cam_pan_duration = std::max(0.f, duration);
    if (scene.cam_pan_duration <= 0.001f) {
        scene.cam_pan = scene.cam_target_pan;
        scene.cam_pan_duration = 0.f;
    }
}

void scene_trigger_shake(SceneState& scene, float intensity, float duration) {
    scene.shake_intensity = intensity;
    scene.shake_duration = duration;
    scene.shake_elapsed = 0.f;
}

void scene_trigger_flash(SceneState& scene, const std::string& hex_color, float duration) {
    scene.flash_color = parse_hex_color(hex_color, sf::Color::White);
    scene.flash_duration = duration;
    scene.flash_elapsed = 0.f;
    scene.flash_alpha = 220.f;
}

void scene_trigger_tint(SceneState& scene, const std::string& hex_color, float duration) {
    scene.tint_start_color = scene.tint_current_color;
    scene.tint_target_color = parse_hex_color(hex_color, sf::Color(0, 0, 0, 0));
    scene.tint_elapsed = 0.f;
    scene.tint_duration = std::max(0.f, duration);
    if (scene.tint_duration <= 0.001f) {
        scene.tint_current_color = scene.tint_target_color;
        scene.tint_duration = 0.f;
    }
}

void scene_set_vignette(SceneState& scene, float alpha) {
    scene.vignette_alpha = std::clamp(alpha, 0.f, 1.f);
}

void scene_set_cinema(SceneState& scene, bool enabled) {
    scene.cinema_enabled = enabled;
}

void scene_set_weather(SceneState& scene, const std::string& type, float density) {
    scene.weather_type = type;
    scene.particles.clear();
    if (type == "none" || density <= 0.f) return;

    size_t count = static_cast<size_t>(std::clamp(density, 5.f, 250.f));
    scene.particles.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        SceneParticle p;
        p.pos = {static_cast<float>(std::rand() % 1320 - 20), static_cast<float>(std::rand() % 760 - 20)};
        if (type == "sakura") {
            p.vel = {40.f + static_cast<float>(std::rand() % 50), 60.f + static_cast<float>(std::rand() % 40)};
            p.rotation = static_cast<float>(std::rand() % 360);
            p.rot_speed = -50.f + static_cast<float>(std::rand() % 100);
            p.size = 6.f + static_cast<float>(std::rand() % 5);
            p.color = sf::Color(254, 205, 211, 200 + (std::rand() % 45));
        } else if (type == "rain") {
            p.vel = {-40.f, 650.f + static_cast<float>(std::rand() % 250)};
            p.rotation = -8.f;
            p.size = 14.f + static_cast<float>(std::rand() % 8);
            p.color = sf::Color(186, 230, 253, 160 + (std::rand() % 60));
        } else if (type == "snow") {
            p.vel = {-15.f + static_cast<float>(std::rand() % 30), 45.f + static_cast<float>(std::rand() % 40)};
            p.size = 3.f + static_cast<float>(std::rand() % 5);
            p.color = sf::Color(241, 245, 249, 180 + (std::rand() % 65));
        } else if (type == "dust") {
            p.vel = {-10.f + static_cast<float>(std::rand() % 20), -20.f - static_cast<float>(std::rand() % 25)};
            p.size = 2.f + static_cast<float>(std::rand() % 3);
            p.color = sf::Color(254, 240, 138, 140 + (std::rand() % 80));
        }
        scene.particles.push_back(p);
    }
}

void scene_show_chapter(SceneState& scene, const std::string& title, const std::string& subtitle, float duration) {
    scene.chapter_title = title;
    scene.chapter_subtitle = subtitle;
    scene.chapter_duration = std::max(1.0f, duration);
    scene.chapter_timer = scene.chapter_duration;
}

void scene_show_toast(SceneState& scene, const std::string& text, float duration) {
    scene.toast_text = text;
    scene.toast_duration = duration;
    scene.toast_timer = duration;
}

void scene_show_achievement_toast(SceneState& scene, const std::string& title, float duration) {
    scene.ach_toast_title = title;
    scene.ach_toast_timer = duration;
}

void scene_set_nvl_mode(SceneState& scene, bool enabled) {
    if (scene.nvl_mode != enabled) {
        scene.nvl_mode = enabled;
        if (!enabled) scene.nvl_history.clear();
    }
}

void scene_clear_nvl(SceneState& scene) {
    scene.nvl_history.clear();
}

void scene_set_dialogue(
    SceneState& scene,
    const QvnConfig& cfg,
    const std::string& name,
    const std::string& text,
    float cps_override,
    const std::string& color_hex,
    const std::string& speaker_id
) {
    if (scene.nvl_mode && !scene.wrapped_dialogue.empty()) {
        std::string prev = scene.speaker_name.empty()
            ? scene.wrapped_dialogue
            : (scene.speaker_name + ": " + scene.wrapped_dialogue);
        scene.nvl_history.push_back(std::move(prev));
        size_t max_nvl = cfg.nvl_max_lines > 0 ? cfg.nvl_max_lines : 6;
        while (scene.nvl_history.size() > max_nvl) scene.nvl_history.erase(scene.nvl_history.begin());
    }

    scene.speaker_name = name;
    scene.speaker_color = parse_hex_color(color_hex, to_sf_color(cfg.name_color));
    scene.active_speaker_id = speaker_id;
    scene.raw_dialogue = text;
    float wrap_w = scene.nvl_mode ? 840.f : cfg.text_max_width;
    scene.wrapped_dialogue = wrap_dialogue_text(scene.font, cfg.text_font_size, wrap_w, text);
    scene.total_chars = qvn_utf8_char_count(scene.wrapped_dialogue.c_str());

    float ny = resolved_name_y(cfg);
    if (scene.text_name) {
        scene.text_name->setString(sf::String::fromUtf8(name.begin(), name.end()));
        scene.text_name->setFillColor(scene.speaker_color);
        scene.text_name->setPosition({cfg.name_x + 14.f, ny});
    }
    if (scene.text_dialogue) {
        scene.text_dialogue->setPosition({cfg.text_x, std::max(cfg.text_y, ny + 34.f)});
    }

    float cps = cps_override > 0.f ? cps_override : cfg.typewriter_cps;
    if (cfg.typewriter_enabled && cps > 0.f && scene.total_chars > 0) {
        scene.typewriter_cps = cps;
        scene.typewriter_elapsed = 0.f;
        scene.visible_chars = 0;
        scene.sound_char_counter = 0;
        scene.typewriter_active = true;
        if (scene.text_dialogue) scene.text_dialogue->setString("");
    } else {
        scene.visible_chars = scene.total_chars;
        scene.typewriter_active = false;
        if (scene.text_dialogue) {
            scene.text_dialogue->setString(sf::String::fromUtf8(scene.wrapped_dialogue.begin(), scene.wrapped_dialogue.end()));
        }
    }
}

bool scene_is_typing(const SceneState& scene) {
    return scene.typewriter_active;
}

void scene_skip_typewriter(SceneState& scene) {
    scene.visible_chars = scene.total_chars;
    scene.typewriter_active = false;
    if (scene.text_dialogue) {
        scene.text_dialogue->setString(sf::String::fromUtf8(scene.wrapped_dialogue.begin(), scene.wrapped_dialogue.end()));
    }
}

void scene_set_choices(SceneState& scene, const QvnConfig& cfg, const std::vector<std::string>& choices, float timer_sec) {
    scene.current_choice_labels = choices;
    scene.choice_timer_total = std::max(0.f, timer_sec);
    scene.choice_timer_remaining = scene.choice_timer_total;
    rebuild_choice_buttons(scene, cfg);
}

void scene_update_choice_hover(SceneState& scene, const sf::Vector2f& mouse_pos) {
    for (auto& btn : scene.choice_buttons) {
        btn.hover_t = btn.rect.contains(mouse_pos) ? 1.f : 0.f;
    }
}

int scene_get_choice_at(const SceneState& scene, const sf::Vector2f& mouse_pos) {
    for (size_t i = 0; i < scene.choice_buttons.size(); ++i) {
        if (scene.choice_buttons[i].rect.contains(mouse_pos)) return static_cast<int>(i);
    }
    return -1;
}

void scene_clear_choices(SceneState& scene) {
    scene.current_choice_labels.clear();
    scene.choice_buttons.clear();
    scene.has_choices = false;
    scene.choice_timer_total = 0.f;
    scene.choice_timer_remaining = 0.f;
}

void scene_update(SceneState& scene, const QvnConfig& cfg, AudioState& audio, float dt) {
    scene.cursor_blink_timer += dt;
    scene.breath_time += dt;

    if (scene.toast_timer > 0.f) scene.toast_timer = std::max(0.f, scene.toast_timer - dt);
    if (scene.ach_toast_timer > 0.f) scene.ach_toast_timer = std::max(0.f, scene.ach_toast_timer - dt);
    if (scene.chapter_timer > 0.f) scene.chapter_timer = std::max(0.f, scene.chapter_timer - dt);

    float target_cinema = scene.cinema_enabled ? 1.f : 0.f;
    scene.cinema_t += (target_cinema - scene.cinema_t) * std::min(1.f, dt * 6.f);

    if (scene.cam_zoom_duration > 0.001f && scene.cam_zoom_elapsed < scene.cam_zoom_duration) {
        scene.cam_zoom_elapsed += dt;
        float t = std::clamp(scene.cam_zoom_elapsed / scene.cam_zoom_duration, 0.f, 1.f);
        scene.cam_zoom = qvn_lerp(scene.cam_start_zoom, scene.cam_target_zoom, qvn_ease_in_out_quad(t));
        if (t >= 1.f) scene.cam_zoom_duration = 0.f;
    }

    if (scene.cam_pan_duration > 0.001f && scene.cam_pan_elapsed < scene.cam_pan_duration) {
        scene.cam_pan_elapsed += dt;
        float t = std::clamp(scene.cam_pan_elapsed / scene.cam_pan_duration, 0.f, 1.f);
        scene.cam_pan.x = qvn_lerp(scene.cam_start_pan.x, scene.cam_target_pan.x, qvn_ease_in_out_quad(t));
        scene.cam_pan.y = qvn_lerp(scene.cam_start_pan.y, scene.cam_target_pan.y, qvn_ease_in_out_quad(t));
        if (t >= 1.f) scene.cam_pan_duration = 0.f;
    }

    if (scene.shake_duration > 0.001f && scene.shake_elapsed < scene.shake_duration) {
        scene.shake_elapsed += dt;
        float progress = scene.shake_elapsed / scene.shake_duration;
        float decay = 1.f - std::clamp(progress, 0.f, 1.f);
        float mag = scene.shake_intensity * decay;
        float rx = (static_cast<float>(std::rand() % 200) / 100.f - 1.f) * mag;
        float ry = (static_cast<float>(std::rand() % 200) / 100.f - 1.f) * mag;
        scene.shake_offset = {rx, ry};
        if (progress >= 1.f) {
            scene.shake_duration = 0.f;
            scene.shake_offset = {0.f, 0.f};
        }
    }

    if (scene.flash_duration > 0.001f && scene.flash_elapsed < scene.flash_duration) {
        scene.flash_elapsed += dt;
        float progress = std::clamp(scene.flash_elapsed / scene.flash_duration, 0.f, 1.f);
        scene.flash_alpha = qvn_lerp(220.f, 0.f, qvn_ease_out_cubic(progress));
        if (progress >= 1.f) {
            scene.flash_duration = 0.f;
            scene.flash_alpha = 0.f;
        }
    }

    if (scene.tint_duration > 0.001f && scene.tint_elapsed < scene.tint_duration) {
        scene.tint_elapsed += dt;
        float t = std::clamp(scene.tint_elapsed / scene.tint_duration, 0.f, 1.f);
        scene.tint_current_color = sf::Color(
            clamp_u8(qvn_lerp(scene.tint_start_color.r, scene.tint_target_color.r, t)),
            clamp_u8(qvn_lerp(scene.tint_start_color.g, scene.tint_target_color.g, t)),
            clamp_u8(qvn_lerp(scene.tint_start_color.b, scene.tint_target_color.b, t)),
            clamp_u8(qvn_lerp(scene.tint_start_color.a, scene.tint_target_color.a, t))
        );
        if (t >= 1.f) scene.tint_duration = 0.f;
    }

    if (!scene.particles.empty()) {
        for (auto& p : scene.particles) {
            p.pos += p.vel * dt;
            p.rotation += p.rot_speed * dt;
            if (p.pos.y > LOGICAL_H + 20.f) {
                p.pos.y = -20.f;
                p.pos.x = static_cast<float>(std::rand() % static_cast<int>(LOGICAL_W + 40.f) - 20);
            }
            if (p.pos.x > LOGICAL_W + 30.f) p.pos.x = -20.f;
            else if (p.pos.x < -30.f) p.pos.x = LOGICAL_W + 20.f;
        }
    }

    if (scene.has_bg && scene.bg_fade_duration > 0.001f && scene.bg_fade_elapsed < scene.bg_fade_duration) {
        scene.bg_fade_elapsed += dt;
        float t = scene.bg_fade_elapsed / scene.bg_fade_duration;
        float eased = qvn_ease_out_cubic(t);
        scene.bg_alpha = qvn_lerp(0.f, 255.f, eased);
        if (scene.bg_sprite) scene.bg_sprite->setColor(sf::Color(255, 255, 255, clamp_u8(scene.bg_alpha)));
        if (t >= 1.f) {
            scene.bg_fade_duration = 0.f;
            scene.has_prev_bg = false;
            scene.prev_bg_sprite.reset();
        }
    }

    for (auto& c : scene.characters) {
        if (!c.active) continue;

        if (c.fade_duration > 0.001f && c.fade_elapsed < c.fade_duration) {
            c.fade_elapsed += dt;
            float t = c.fade_elapsed / c.fade_duration;
            float eased = qvn_ease_out_cubic(t);
            c.alpha = qvn_lerp(c.start_alpha, c.target_alpha, eased);
            if (c.has_prev && c.prev_sprite) {
                c.prev_sprite->setColor(sf::Color(255, 255, 255, clamp_u8(qvn_lerp(255.f, 0.f, eased))));
            }
            if (t >= 1.f) {
                c.fade_duration = 0.f;
                c.has_prev = false;
                c.prev_sprite.reset();
                if (c.hiding) {
                    c.active = false;
                    c.sprite.reset();
                    c.hiding = false;
                    continue;
                }
            }
        }

        float target_b = 255.f;
        if (cfg.dim_inactive_chars && !scene.active_speaker_id.empty() && c.id != scene.active_speaker_id) {
            target_b = static_cast<float>(cfg.inactive_char_brightness);
        }
        c.brightness += (target_b - c.brightness) * std::min(1.f, dt * 10.f);
        uint8_t br = clamp_u8(c.brightness);
        if (c.sprite) c.sprite->setColor(sf::Color(br, br, br, clamp_u8(c.alpha)));

        if (c.move_duration > 0.001f && c.move_elapsed < c.move_duration) {
            c.move_elapsed += dt;
            float t = c.move_elapsed / c.move_duration;
            float eased = qvn_ease_in_out_quad(t);
            c.pos.x = qvn_lerp(c.start_pos.x, c.target_pos.x, eased);
            c.pos.y = qvn_lerp(c.start_pos.y, c.target_pos.y, eased);
            if (t >= 1.f) c.move_duration = 0.f;
        }

        if (c.scale_duration > 0.001f && c.scale_elapsed < c.scale_duration) {
            c.scale_elapsed += dt;
            float t = std::clamp(c.scale_elapsed / c.scale_duration, 0.f, 1.f);
            c.scale = qvn_lerp(c.start_scale, c.target_scale, qvn_ease_out_cubic(t));
            if (t >= 1.f) c.scale_duration = 0.f;
        }

        float bounce_y = 0.f;
        if (c.bounce_timer > 0.f) {
            c.bounce_timer = std::max(0.f, c.bounce_timer - dt);
            float bt = c.bounce_timer / 0.36f;
            bounce_y = -std::sin(bt * 3.1415926f) * 26.f;
        }

        float breath_offset = (cfg.idle_breathing && !c.hiding) ? (std::sin(scene.breath_time * 2.2f + c.breath_phase) * 2.2f) : 0.f;

        if (c.sprite) {
            c.sprite->setPosition({c.pos.x, c.pos.y + bounce_y + breath_offset});
            c.sprite->setScale({c.flip_x ? -c.scale : c.scale, c.scale});
        }
        if (c.has_prev && c.prev_sprite) {
            c.prev_sprite->setPosition({c.pos.x, c.pos.y + bounce_y + breath_offset});
            c.prev_sprite->setScale({c.flip_x ? -c.scale : c.scale, c.scale});
        }
    }

    if (scene.typewriter_active) {
        scene.typewriter_elapsed += dt;
        size_t target = static_cast<size_t>(scene.typewriter_elapsed * scene.typewriter_cps);
        if (target >= scene.total_chars) {
            scene_skip_typewriter(scene);
        } else if (target > scene.visible_chars) {
            scene.visible_chars = target;
            size_t bytes = qvn_utf8_prefix_bytes(scene.wrapped_dialogue.c_str(), scene.visible_chars);
            if (scene.text_dialogue) {
                scene.text_dialogue->setString(sf::String::fromUtf8(
                    scene.wrapped_dialogue.begin(),
                    scene.wrapped_dialogue.begin() + static_cast<std::ptrdiff_t>(bytes)
                ));
            }
            if (cfg.typewriter_sound[0] != '\0') {
                uint32_t interval = cfg.typewriter_sound_interval > 0 ? cfg.typewriter_sound_interval : 1;
                if (scene.sound_char_counter % interval == 0) audio_play_sound(audio, cfg.typewriter_sound);
                scene.sound_char_counter++;
            }
        }
    }
}

void scene_render(const SceneState& scene, const QvnConfig& cfg, UiContext& ui, sf::RenderWindow& window, bool show_dialogue) {
    sf::Transform cam_tf;
    cam_tf.translate({(LOGICAL_W * 0.5f) + scene.shake_offset.x + scene.cam_pan.x, (LOGICAL_H * 0.5f) + scene.shake_offset.y + scene.cam_pan.y});
    cam_tf.scale({scene.cam_zoom, scene.cam_zoom});
    cam_tf.translate({-LOGICAL_W * 0.5f, -LOGICAL_H * 0.5f});
    sf::RenderStates states(cam_tf);

    if (scene.has_prev_bg && scene.prev_bg_sprite) window.draw(*scene.prev_bg_sprite, states);
    if (scene.has_bg && scene.bg_sprite) window.draw(*scene.bg_sprite, states);

    for (const auto& c : scene.characters) {
        if (!c.active) continue;
        if (c.has_prev && c.prev_sprite) window.draw(*c.prev_sprite, states);
        if (c.sprite) window.draw(*c.sprite, states);
    }

    if (!scene.particles.empty()) {
        for (const auto& p : scene.particles) {
            if (scene.weather_type == "sakura") {
                sf::ConvexShape petal(4);
                petal.setPoint(0, {-p.size * 0.5f, -p.size});
                petal.setPoint(1, {p.size * 0.5f, -p.size * 0.5f});
                petal.setPoint(2, {0.f, p.size});
                petal.setPoint(3, {-p.size * 0.5f, 0.f});
                petal.setPosition(p.pos);
                petal.setRotation(sf::degrees(p.rotation));
                petal.setFillColor(p.color);
                window.draw(petal, states);
            } else if (scene.weather_type == "rain") {
                sf::RectangleShape drop({1.5f, p.size});
                drop.setPosition(p.pos);
                drop.setRotation(sf::degrees(p.rotation));
                drop.setFillColor(p.color);
                window.draw(drop, states);
            } else {
                sf::CircleShape dot(p.size * 0.5f);
                dot.setPosition(p.pos);
                dot.setFillColor(p.color);
                window.draw(dot, states);
            }
        }
    }

    if (scene.tint_current_color.a > 1) {
        sf::RectangleShape tint_rect({LOGICAL_W, LOGICAL_H});
        tint_rect.setFillColor(scene.tint_current_color);
        window.draw(tint_rect);
    }

    if (scene.vignette_alpha > 0.01f || cfg.vignette_strength > 0.01f) {
        float va = std::max(scene.vignette_alpha, cfg.vignette_strength);
        uint8_t a = clamp_u8(va * 190.f);
        ui_draw_horizontal_gradient(window, sf::FloatRect({0.f, 0.f}, {160.f, LOGICAL_H}), sf::Color(0, 0, 0, a), sf::Color(0, 0, 0, 0));
        ui_draw_horizontal_gradient(window, sf::FloatRect({LOGICAL_W - 160.f, 0.f}, {160.f, LOGICAL_H}), sf::Color(0, 0, 0, 0), sf::Color(0, 0, 0, a));
        ui_draw_gradient_rect(window, sf::FloatRect({0.f, 0.f}, {LOGICAL_W, 110.f}), sf::Color(0, 0, 0, a), sf::Color(0, 0, 0, 0));
        ui_draw_gradient_rect(window, sf::FloatRect({0.f, LOGICAL_H - 110.f}, {LOGICAL_W, 110.f}), sf::Color(0, 0, 0, 0), sf::Color(0, 0, 0, a));
    }

    if (scene.cinema_t > 0.001f) {
        float bar_h = 72.f * scene.cinema_t;
        sf::RectangleShape top_bar({LOGICAL_W, bar_h});
        top_bar.setFillColor(sf::Color(0, 0, 0));
        window.draw(top_bar);
        sf::RectangleShape bot_bar({LOGICAL_W, bar_h});
        bot_bar.setPosition({0.f, LOGICAL_H - bar_h});
        bot_bar.setFillColor(sf::Color(0, 0, 0));
        window.draw(bot_bar);
    }

    if (scene.flash_alpha > 1.f) {
        sf::RectangleShape flash_rect({LOGICAL_W, LOGICAL_H});
        flash_rect.setFillColor(sf::Color(scene.flash_color.r, scene.flash_color.g, scene.flash_color.b, clamp_u8(scene.flash_alpha)));
        window.draw(flash_rect);
    }

    if (show_dialogue) {
        if (scene.nvl_mode) {
            sf::RectangleShape nvl_bg({LOGICAL_W, LOGICAL_H});
            nvl_bg.setFillColor(to_sf_color(cfg.nvl_bg_color));
            window.draw(nvl_bg);

            float y = cfg.nvl_y;
            float col_x = cfg.nvl_x;
            for (const auto& prev : scene.nvl_history) {
                sf::Text pt(scene.font, sf::String::fromUtf8(prev.begin(), prev.end()), cfg.text_font_size);
                pt.setLineSpacing(1.4f);
                pt.setFillColor(sf::Color(161, 161, 170, 195));
                pt.setPosition({col_x, y});
                window.draw(pt);
                y += pt.getLocalBounds().size.y + 28.f;
            }
            if (scene.text_dialogue) {
                sf::Text cur(*scene.text_dialogue);
                cur.setPosition({col_x, y});
                cur.setFillColor(sf::Color(250, 250, 250));
                window.draw(cur);
            }
            if (!scene.typewriter_active && !scene.has_choices) {
                float bob = std::sin(scene.cursor_blink_timer * 5.f) * 3.f;
                ui_draw_icon(window, UiIcon::ChevronDown, {1060.f, 650.f + bob}, 14.f, to_sf_color(cfg.theme_accent_hover_color));
            }
        } else {
            sf::FloatRect box_rect({cfg.box_x, cfg.box_y}, {cfg.box_width, cfg.box_height});
            ui_draw_glass_panel(
                window,
                box_rect,
                cfg.theme_corner_radius,
                to_sf_color(cfg.box_color),
                to_sf_color(cfg.box_outline_color),
                cfg.theme_shadow_alpha
            );

            if (!scene.speaker_name.empty() && scene.text_name) {
                float ny = resolved_name_y(cfg);
                sf::FloatRect bar({cfg.name_x, ny + 3.f}, {3.5f, 20.f});
                ui_draw_rounded_panel(window, bar, 1.5f, scene.speaker_color, sf::Color::Transparent, 0.f, 0);
                window.draw(*scene.text_name);
            }

            if (scene.text_dialogue) window.draw(*scene.text_dialogue);

            if (!scene.typewriter_active && !scene.has_choices) {
                float bob = std::sin(scene.cursor_blink_timer * 5.f) * 3.f;
                float pulse = 0.5f + 0.5f * std::sin(scene.cursor_blink_timer * 5.f);
                sf::Color ind_col = to_sf_color(cfg.theme_accent_hover_color);
                ind_col.a = clamp_u8(130.f + pulse * 125.f);
                ui_draw_icon(window, UiIcon::ChevronDown, {cfg.box_x + cfg.box_width - 32.f, cfg.box_y + cfg.box_height - 26.f + bob}, 13.f, ind_col);
            }
        }
    }

    if (scene.has_choices) {
        if (scene.choice_timer_total > 0.f && !scene.choice_buttons.empty()) {
            float ratio = std::clamp(scene.choice_timer_remaining / scene.choice_timer_total, 0.f, 1.f);
            float bw = cfg.choice_width;
            float bx = (LOGICAL_W - bw) * 0.5f;
            float by = scene.choice_buttons[0].rect.position.y - 22.f;
            ui_draw_rounded_panel(window, sf::FloatRect({bx, by}, {bw, 8.f}), 4.f, sf::Color(24, 26, 36, 220), sf::Color(255, 255, 255, 24), 1.f, 0);
            sf::Color bar_c = ratio < 0.3f ? sf::Color(239, 68, 68) : to_sf_color(cfg.theme_accent_color);
            ui_draw_rounded_panel(window, sf::FloatRect({bx, by}, {std::max(8.f, bw * ratio), 8.f}), 4.f, bar_c, sf::Color::Transparent, 0.f, 0);
        }

        for (size_t i = 0; i < scene.choice_buttons.size(); ++i) {
            const auto& btn = scene.choice_buttons[i];
            float anim = ui_anim_hover(ui, 60000ULL + i, btn.hover_t > 0.5f);
            sf::FloatRect r = btn.rect;
            r.position.x += anim * 6.f;

            sf::Color base_bg = to_sf_color(cfg.choice_bg_color);
            sf::Color hov_bg = to_sf_color(cfg.choice_hover_color);
            sf::Color base_br = to_sf_color(cfg.choice_outline_color);
            sf::Color hov_br = to_sf_color(cfg.choice_hover_outline_color);

            sf::Color bg(
                clamp_u8(base_bg.r + (hov_bg.r - base_bg.r) * anim),
                clamp_u8(base_bg.g + (hov_bg.g - base_bg.g) * anim),
                clamp_u8(base_bg.b + (hov_bg.b - base_bg.b) * anim),
                clamp_u8(base_bg.a + (hov_bg.a - base_bg.a) * anim)
            );
            sf::Color br(
                clamp_u8(base_br.r + (hov_br.r - base_br.r) * anim),
                clamp_u8(base_br.g + (hov_br.g - base_br.g) * anim),
                clamp_u8(base_br.b + (hov_br.b - base_br.b) * anim),
                clamp_u8(base_br.a + (hov_br.a - base_br.a) * anim)
            );

            ui_draw_glass_panel(window, r, 12.f, bg, br, static_cast<uint8_t>(55.f + anim * 50.f));

            sf::FloatRect num_pill({r.position.x + 14.f, r.position.y + (r.size.y - 24.f) * 0.5f}, {24.f, 24.f});
            sf::Color pill_col = anim > 0.2f ? to_sf_color(cfg.theme_accent_color) : sf::Color(255, 255, 255, 20);
            ui_draw_rounded_panel(window, num_pill, 12.f, pill_col, sf::Color::Transparent, 0.f, 0);
            ui_draw_label_centered(ui, window, std::to_string(i + 1), num_pill, 12, sf::Color(244, 244, 245));

            ui_draw_label_centered(ui, window, btn.label, r, cfg.choice_font_size, to_sf_color(cfg.choice_text_color));
        }
    }

    if (scene.chapter_timer > 0.f && !scene.chapter_title.empty()) {
        float fade_in = std::min(1.f, (scene.chapter_duration - scene.chapter_timer) / 0.45f);
        float fade_out = std::min(1.f, scene.chapter_timer / 0.45f);
        float alpha_f = std::min(fade_in, fade_out);
        uint8_t a = clamp_u8(alpha_f * 235.f);
        sf::RectangleShape backdrop({LOGICAL_W, LOGICAL_H});
        backdrop.setFillColor(sf::Color(8, 10, 15, clamp_u8(alpha_f * 200.f)));
        window.draw(backdrop);

        ui_draw_label_centered(ui, window, scene.chapter_title, sf::FloatRect({0.f, 280.f}, {LOGICAL_W, 56.f}), 44, sf::Color(250, 250, 250, a));
        if (!scene.chapter_subtitle.empty()) {
            ui_draw_label_centered(ui, window, scene.chapter_subtitle, sf::FloatRect({0.f, 350.f}, {LOGICAL_W, 34.f}), 18, sf::Color(161, 161, 170, a));
        }
    }

    if (scene.toast_timer > 0.f && !scene.toast_text.empty()) {
        float alpha_f = std::min(1.f, scene.toast_timer / 0.35f);
        uint8_t a = clamp_u8(alpha_f * 235.f);
        sf::FloatRect toast_rect({(LOGICAL_W * 0.5f) - 150.f, 24.f}, {300.f, 36.f});
        ui_draw_glass_panel(
            window,
            toast_rect,
            18.f,
            sf::Color(15, 16, 22, a),
            sf::Color(255, 255, 255, static_cast<uint8_t>(a / 6)),
            static_cast<uint8_t>(alpha_f * 70.f)
        );
        ui_draw_label_centered(ui, window, scene.toast_text, toast_rect, 14, sf::Color(244, 244, 245, a));
    }

    if (scene.ach_toast_timer > 0.f && !scene.ach_toast_title.empty()) {
        float alpha_f = std::min(1.f, scene.ach_toast_timer / 0.4f);
        uint8_t a = clamp_u8(alpha_f * 240.f);
        sf::FloatRect ach_rect({LOGICAL_W - 344.f, 24.f}, {320.f, 64.f});
        ui_draw_glass_panel(
            window,
            ach_rect,
            14.f,
            sf::Color(18, 20, 28, a),
            sf::Color(250, 204, 21, static_cast<uint8_t>(a / 2)),
            static_cast<uint8_t>(alpha_f * 85.f)
        );
        ui_draw_icon(window, UiIcon::Trophy, {ach_rect.position.x + 30.f, ach_rect.position.y + 32.f}, 20.f, sf::Color(250, 204, 21, a));
        ui_draw_label(ui, window, "Achievement Unlocked", {ach_rect.position.x + 54.f, ach_rect.position.y + 12.f}, 11, sf::Color(250, 204, 21, a));
        ui_draw_label(ui, window, scene.ach_toast_title, {ach_rect.position.x + 54.f, ach_rect.position.y + 30.f}, 15, sf::Color(248, 250, 252, a));
    }
}