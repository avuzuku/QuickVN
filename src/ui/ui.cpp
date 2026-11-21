#include "ui/ui.hpp"
#include "platform/platform.hpp"
#include "rust_core.h"
#include <algorithm>
#include <cmath>

static uint64_t hash_rect_id(sf::FloatRect rect, const std::string& tag) {
    uint64_t h = 0xCBF29CE484222325ULL;
    auto mix = [&](uint64_t v) {
        h ^= v;
        h *= 0x100000001B3ULL;
    };
    mix(static_cast<uint64_t>(std::round(rect.position.x * 10.f)));
    mix(static_cast<uint64_t>(std::round(rect.position.y * 10.f)));
    for (unsigned char c : tag) mix(c);
    return h;
}

static uint64_t hash_text_key(const std::string& s, uint32_t size, uint32_t salt = 0) {
    uint64_t h = 0xCBF29CE484222325ULL;
    h ^= static_cast<uint64_t>(size) * 0x9E3779B97F4A7C15ULL;
    h ^= static_cast<uint64_t>(salt) * 0xBF58476D1CE4E5B9ULL;
    for (unsigned char c : s) {
        h ^= c;
        h *= 0x100000001B3ULL;
    }
    return h;
}

static sf::Color lerp_color(sf::Color a, sf::Color b, float t) {
    float c = std::clamp(t, 0.f, 1.f);
    return sf::Color(
        static_cast<uint8_t>(a.r + (b.r - a.r) * c),
        static_cast<uint8_t>(a.g + (b.g - a.g) * c),
        static_cast<uint8_t>(a.b + (b.b - a.b) * c),
        static_cast<uint8_t>(a.a + (b.a - a.a) * c)
    );
}

static void pop_utf8_char(std::string& s) {
    while (!s.empty() && (static_cast<unsigned char>(s.back()) & 0xC0) == 0x80) {
        s.pop_back();
    }
    if (!s.empty()) s.pop_back();
}

static UiCachedText& ui_get_cached_text(UiContext& ui, const std::string& text, uint32_t size) {
    uint64_t key = hash_text_key(text, size);
    auto it = ui.text_cache.find(key);
    if (it != ui.text_cache.end()) return it->second;
    if (ui.text_cache.size() > 512) ui.text_cache.clear();
    sf::Text t(ui.font, sf::String::fromUtf8(text.begin(), text.end()), size);
    sf::FloatRect b = t.getLocalBounds();
    auto res = ui.text_cache.emplace(key, UiCachedText{std::move(t), b});
    return res.first->second;
}

static float ui_cached_width(UiContext& ui, const std::string& text, uint32_t size) {
    if (text.empty()) return 0.f;
    UiCachedText& c = ui_get_cached_text(ui, text, size);
    return c.bounds.position.x + c.bounds.size.x;
}

static const std::string& ui_cached_truncate(UiContext& ui, const std::string& text, uint32_t size, float max_width) {
    static const std::string empty_s;
    if (max_width <= 8.f || text.empty()) return empty_s;
    uint32_t q_w = static_cast<uint32_t>(std::round(max_width));
    uint64_t key = hash_text_key(text, size, q_w);
    auto it = ui.trunc_cache.find(key);
    if (it != ui.trunc_cache.end()) return it->second;
    if (ui.trunc_cache.size() > 512) ui.trunc_cache.clear();
    std::string fitted = ui_cached_width(ui, text, size) <= max_width ? text : ui_truncate_text(ui.font, text, size, max_width);
    auto res = ui.trunc_cache.emplace(key, std::move(fitted));
    return res.first->second;
}

bool ui_init(UiContext& ui, const std::string& font_path) {
    ui.text_cache.clear();
    ui.trunc_cache.clear();
    if (!font_path.empty()) {
        std::vector<uint8_t> raw;
        if (platform_read_bytes(font_path, raw) && !raw.empty()) {
            ui.font_buffer = std::move(raw);
            if (ui.font.openFromMemory(ui.font_buffer.data(), ui.font_buffer.size())) return true;
        }
    }
    size_t builtin_len = 0;
    const uint8_t* builtin_ptr = qvn_builtin_font_data(&builtin_len);
    if (builtin_ptr && builtin_len > 0) {
        ui.font_buffer.assign(builtin_ptr, builtin_ptr + builtin_len);
        if (ui.font.openFromMemory(ui.font_buffer.data(), ui.font_buffer.size())) return true;
    }
    const char* fallbacks[] = {
        "assets/fonts/font.ttf",
        "assets/font.ttf",
        "/usr/share/fonts/TTF/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/System/Library/Fonts/Helvetica.ttc",
        "C:/Windows/Fonts/segoeui.ttf"
    };
    for (const char* path : fallbacks) {
        if (ui.font.openFromFile(path)) return true;
    }
    return false;
}

void ui_reset_frame(UiContext& ui, float dt) {
    ui.mouse_clicked = false;
    ui.mouse_released = false;
    ui.wheel_delta = 0.f;
    ui.text_entered_utf8.clear();
    ui.backspace_pressed = false;
    ui.dt = std::clamp(dt, 0.001f, 0.05f);
    ui.blink_timer += ui.dt;
    for (auto& kv : ui.press_anims) {
        if (kv.second > 0.f) kv.second = std::max(0.f, kv.second - ui.dt * 7.5f);
    }
}

void ui_consume_click(UiContext& ui) {
    ui.mouse_clicked = false;
}

void ui_handle_event(UiContext& ui, const sf::Event& event, const sf::RenderWindow& window) {
    if (const auto* moved = event.getIf<sf::Event::MouseMoved>()) {
        ui.mouse_pos = window.mapPixelToCoords(moved->position);
    } else if (const auto* btn = event.getIf<sf::Event::MouseButtonPressed>()) {
        if (btn->button == sf::Mouse::Button::Left) {
            ui.mouse_pos = window.mapPixelToCoords(btn->position);
            ui.mouse_down = true;
            ui.mouse_clicked = true;
        }
    } else if (const auto* btn_rel = event.getIf<sf::Event::MouseButtonReleased>()) {
        if (btn_rel->button == sf::Mouse::Button::Left) {
            ui.mouse_pos = window.mapPixelToCoords(btn_rel->position);
            ui.mouse_down = false;
            ui.mouse_released = true;
            ui.active_slider_id = -1;
        }
    } else if (const auto* touch_moved = event.getIf<sf::Event::TouchMoved>()) {
        ui.mouse_pos = window.mapPixelToCoords(touch_moved->position);
    } else if (const auto* touch_began = event.getIf<sf::Event::TouchBegan>()) {
        ui.mouse_pos = window.mapPixelToCoords(touch_began->position);
        ui.mouse_down = true;
        ui.mouse_clicked = true;
    } else if (const auto* touch_ended = event.getIf<sf::Event::TouchEnded>()) {
        ui.mouse_pos = window.mapPixelToCoords(touch_ended->position);
        ui.mouse_down = false;
        ui.mouse_released = true;
        ui.active_slider_id = -1;
    } else if (const auto* wheel = event.getIf<sf::Event::MouseWheelScrolled>()) {
        ui.wheel_delta += wheel->delta;
    } else if (const auto* txt = event.getIf<sf::Event::TextEntered>()) {
        uint32_t cp = txt->unicode;
        if (cp == 8) {
            ui.backspace_pressed = true;
        } else if (cp >= 32 && cp != 127) {
            if (cp < 0x80) {
                ui.text_entered_utf8.push_back(static_cast<char>(cp));
            } else if (cp < 0x800) {
                ui.text_entered_utf8.push_back(static_cast<char>(0xC0 | (cp >> 6)));
                ui.text_entered_utf8.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
            } else if (cp < 0x10000) {
                ui.text_entered_utf8.push_back(static_cast<char>(0xE0 | (cp >> 12)));
                ui.text_entered_utf8.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
                ui.text_entered_utf8.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
            } else {
                ui.text_entered_utf8.push_back(static_cast<char>(0xF0 | (cp >> 18)));
                ui.text_entered_utf8.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
                ui.text_entered_utf8.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
                ui.text_entered_utf8.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
            }
        }
    }
}

float ui_anim_hover(UiContext& ui, uint64_t id, bool hovered) {
    float& cur = ui.hover_anims[id];
    float target = hovered ? 1.f : 0.f;
    float step = std::min(1.f, ui.dt * 18.f);
    cur += (target - cur) * step;
    return cur;
}

float ui_measure_text_width(const sf::Font& font, const std::string& text, uint32_t size) {
    if (text.empty()) return 0.f;
    sf::Text txt(font, sf::String::fromUtf8(text.begin(), text.end()), size);
    sf::FloatRect b = txt.getLocalBounds();
    return b.position.x + b.size.x;
}

std::string ui_truncate_text(const sf::Font& font, const std::string& text, uint32_t size, float max_width) {
    if (max_width <= 8.f || text.empty()) return "";
    if (ui_measure_text_width(font, text, size) <= max_width) return text;
    std::string out = text;
    while (!out.empty()) {
        pop_utf8_char(out);
        std::string cand = out + "...";
        if (ui_measure_text_width(font, cand, size) <= max_width) return cand;
    }
    return "...";
}

void ui_draw_divider(sf::RenderWindow& window, sf::Vector2f pos, float width, sf::Color color) {
    sf::RectangleShape line({std::max(0.f, width), 1.f});
    line.setPosition({std::round(pos.x), std::round(pos.y)});
    line.setFillColor(color);
    window.draw(line);
}

sf::ConvexShape ui_make_rounded_rect(sf::FloatRect rect, float radius, int corner_pts) {
    float r = std::min({radius, 4.f, rect.size.x * 0.5f, rect.size.y * 0.5f});
    if (r < 0.75f || corner_pts < 2) {
        sf::ConvexShape box(4);
        box.setPoint(0, {rect.position.x, rect.position.y});
        box.setPoint(1, {rect.position.x + rect.size.x, rect.position.y});
        box.setPoint(2, {rect.position.x + rect.size.x, rect.position.y + rect.size.y});
        box.setPoint(3, {rect.position.x, rect.position.y + rect.size.y});
        return box;
    }

    int pts = std::min(corner_pts, 5);
    int total = pts * 4;
    sf::ConvexShape shape(static_cast<size_t>(total));
    const float pi_half = 1.57079632679f;
    sf::Vector2f centers[4] = {
        {rect.position.x + rect.size.x - r, rect.position.y + r},
        {rect.position.x + rect.size.x - r, rect.position.y + rect.size.y - r},
        {rect.position.x + r, rect.position.y + rect.size.y - r},
        {rect.position.x + r, rect.position.y + r}
    };
    float start_angles[4] = {-pi_half, 0.f, pi_half, pi_half * 2.f};

    size_t idx = 0;
    for (int c = 0; c < 4; ++c) {
        for (int p = 0; p < pts; ++p) {
            float t = static_cast<float>(p) / static_cast<float>(pts - 1);
            float ang = start_angles[c] + t * pi_half;
            shape.setPoint(idx++, {centers[c].x + std::cos(ang) * r, centers[c].y + std::sin(ang) * r});
        }
    }
    return shape;
}

void ui_draw_gradient_rect(sf::RenderWindow& window, sf::FloatRect rect, sf::Color top_color, sf::Color bottom_color) {
    sf::VertexArray quad(sf::PrimitiveType::TriangleStrip, 4);
    quad[0].position = {rect.position.x, rect.position.y};
    quad[0].color = top_color;
    quad[1].position = {rect.position.x + rect.size.x, rect.position.y};
    quad[1].color = top_color;
    quad[2].position = {rect.position.x, rect.position.y + rect.size.y};
    quad[2].color = bottom_color;
    quad[3].position = {rect.position.x + rect.size.x, rect.position.y + rect.size.y};
    quad[3].color = bottom_color;
    window.draw(quad);
}

void ui_draw_horizontal_gradient(sf::RenderWindow& window, sf::FloatRect rect, sf::Color left_color, sf::Color right_color) {
    sf::VertexArray quad(sf::PrimitiveType::TriangleStrip, 4);
    quad[0].position = {rect.position.x, rect.position.y};
    quad[0].color = left_color;
    quad[1].position = {rect.position.x + rect.size.x, rect.position.y};
    quad[1].color = right_color;
    quad[2].position = {rect.position.x, rect.position.y + rect.size.y};
    quad[2].color = left_color;
    quad[3].position = {rect.position.x + rect.size.x, rect.position.y + rect.size.y};
    quad[3].color = right_color;
    window.draw(quad);
}

void ui_draw_rounded_panel(
    sf::RenderWindow& window,
    sf::FloatRect rect,
    float radius,
    sf::Color fill,
    sf::Color outline,
    float thickness,
    uint8_t shadow_alpha
) {
    (void)shadow_alpha;
    sf::ConvexShape shape = ui_make_rounded_rect(rect, std::min(radius, 4.f), 4);
    shape.setFillColor(fill);
    if (thickness > 0.f && outline.a > 0) {
        shape.setOutlineColor(outline);
        shape.setOutlineThickness(thickness);
    }
    window.draw(shape);
}

void ui_draw_glass_panel(
    sf::RenderWindow& window,
    sf::FloatRect rect,
    float radius,
    sf::Color fill,
    sf::Color outline,
    uint8_t shadow_alpha
) {
    (void)shadow_alpha;
    ui_draw_rounded_panel(window, rect, std::min(radius, 4.f), fill, outline, 1.f, 0);
}

void ui_draw_panel(sf::RenderWindow& window, sf::FloatRect rect, sf::Color fill, sf::Color outline, float thickness) {
    ui_draw_rounded_panel(window, rect, 3.f, fill, outline, thickness, 0);
}

void ui_draw_label(UiContext& ui, sf::RenderWindow& window, const std::string& text, sf::Vector2f pos, uint32_t size, sf::Color color) {
    if (text.empty()) return;
    UiCachedText& c = ui_get_cached_text(ui, text, size);
    c.text.setFillColor(color);
    c.text.setPosition({std::round(pos.x), std::round(pos.y)});
    window.draw(c.text);
}

void ui_draw_label_clamped(UiContext& ui, sf::RenderWindow& window, const std::string& text, sf::Vector2f pos, uint32_t size, sf::Color color, float max_width) {
    const std::string& fitted = ui_cached_truncate(ui, text, size, max_width);
    ui_draw_label(ui, window, fitted, pos, size, color);
}

void ui_draw_label_centered(UiContext& ui, sf::RenderWindow& window, const std::string& text, sf::FloatRect rect, uint32_t size, sf::Color color) {
    const std::string& fitted = ui_cached_truncate(ui, text, size, std::max(8.f, rect.size.x - 12.f));
    if (fitted.empty()) return;
    UiCachedText& c = ui_get_cached_text(ui, fitted, size);
    c.text.setFillColor(color);
    float tx = rect.position.x + (rect.size.x - c.bounds.size.x) * 0.5f - c.bounds.position.x;
    float ty = rect.position.y + (rect.size.y - c.bounds.size.y) * 0.5f - c.bounds.position.y;
    c.text.setPosition({std::round(tx), std::round(ty)});
    window.draw(c.text);
}

void ui_draw_soft_badge(UiContext& ui, sf::RenderWindow& window, sf::FloatRect rect, const std::string& text, sf::Color accent, uint32_t font_size) {
    sf::Color fill(accent.r, accent.g, accent.b, 20);
    sf::Color border(accent.r, accent.g, accent.b, 95);
    sf::Color txt_col(
        static_cast<uint8_t>(std::min(255, static_cast<int>(accent.r) + 55)),
        static_cast<uint8_t>(std::min(255, static_cast<int>(accent.g) + 55)),
        static_cast<uint8_t>(std::min(255, static_cast<int>(accent.b) + 55)),
        255
    );
    ui_draw_rounded_panel(window, rect, 2.f, fill, border, 1.f, 0);
    ui_draw_label_centered(ui, window, text, rect, font_size, txt_col);
}

static void draw_stroke(sf::RenderWindow& window, sf::Vector2f a, sf::Vector2f b, float thickness, sf::Color color) {
    sf::Vector2f d = b - a;
    float len = std::sqrt(d.x * d.x + d.y * d.y);
    if (len < 0.001f) return;
    float ang = std::atan2(d.y, d.x) * 57.2957795f;
    sf::RectangleShape line({len, thickness});
    line.setOrigin({0.f, thickness * 0.5f});
    line.setPosition(a);
    line.setRotation(sf::degrees(ang));
    line.setFillColor(color);
    window.draw(line);
}

void ui_draw_icon(sf::RenderWindow& window, UiIcon icon, sf::Vector2f center, float size, sf::Color color) {
    float r = size * 0.5f;
    switch (icon) {
        case UiIcon::None:
            break;
        case UiIcon::Play: {
            sf::ConvexShape tri(3);
            tri.setPoint(0, {center.x - r * 0.4f, center.y - r * 0.68f});
            tri.setPoint(1, {center.x + r * 0.68f, center.y});
            tri.setPoint(2, {center.x - r * 0.4f, center.y + r * 0.68f});
            tri.setFillColor(color);
            window.draw(tri);
            break;
        }
        case UiIcon::Code: {
            float t = std::max(1.4f, size * 0.1f);
            draw_stroke(window, {center.x - r * 0.2f, center.y - r * 0.52f}, {center.x - r * 0.72f, center.y}, t, color);
            draw_stroke(window, {center.x - r * 0.72f, center.y}, {center.x - r * 0.2f, center.y + r * 0.52f}, t, color);
            draw_stroke(window, {center.x + r * 0.2f, center.y - r * 0.52f}, {center.x + r * 0.72f, center.y}, t, color);
            draw_stroke(window, {center.x + r * 0.72f, center.y}, {center.x + r * 0.2f, center.y + r * 0.52f}, t, color);
            break;
        }
        case UiIcon::Sliders: {
            float t = std::max(1.4f, size * 0.09f);
            draw_stroke(window, {center.x - r * 0.72f, center.y - r * 0.46f}, {center.x + r * 0.72f, center.y - r * 0.46f}, t, color);
            draw_stroke(window, {center.x - r * 0.72f, center.y}, {center.x + r * 0.72f, center.y}, t, color);
            draw_stroke(window, {center.x - r * 0.72f, center.y + r * 0.46f}, {center.x + r * 0.72f, center.y + r * 0.46f}, t, color);
            sf::RectangleShape k({r * 0.26f, r * 0.26f});
            k.setOrigin({r * 0.13f, r * 0.13f});
            k.setFillColor(color);
            k.setPosition({center.x - r * 0.25f, center.y - r * 0.46f});
            window.draw(k);
            k.setPosition({center.x + r * 0.35f, center.y});
            window.draw(k);
            k.setPosition({center.x - r * 0.1f, center.y + r * 0.46f});
            window.draw(k);
            break;
        }
        case UiIcon::Folder: {
            float t = std::max(1.4f, size * 0.09f);
            sf::RectangleShape body({size * 0.76f, size * 0.52f});
            body.setPosition({center.x - r * 0.76f, center.y - r * 0.24f});
            body.setFillColor(sf::Color::Transparent);
            body.setOutlineColor(color);
            body.setOutlineThickness(t);
            window.draw(body);
            draw_stroke(window, {center.x - r * 0.76f, center.y - r * 0.24f}, {center.x - r * 0.76f, center.y - r * 0.54f}, t, color);
            draw_stroke(window, {center.x - r * 0.76f, center.y - r * 0.54f}, {center.x - r * 0.12f, center.y - r * 0.54f}, t, color);
            draw_stroke(window, {center.x - r * 0.12f, center.y - r * 0.54f}, {center.x + r * 0.06f, center.y - r * 0.24f}, t, color);
            break;
        }
        case UiIcon::Layers: {
            float t = std::max(1.4f, size * 0.1f);
            draw_stroke(window, {center.x, center.y - r * 0.68f}, {center.x + r * 0.72f, center.y - r * 0.2f}, t, color);
            draw_stroke(window, {center.x + r * 0.72f, center.y - r * 0.2f}, {center.x, center.y + r * 0.24f}, t, color);
            draw_stroke(window, {center.x, center.y + r * 0.24f}, {center.x - r * 0.72f, center.y - r * 0.2f}, t, color);
            draw_stroke(window, {center.x - r * 0.72f, center.y - r * 0.2f}, {center.x, center.y - r * 0.68f}, t, color);
            draw_stroke(window, {center.x - r * 0.72f, center.y + r * 0.24f}, {center.x, center.y + r * 0.68f}, t, color);
            draw_stroke(window, {center.x, center.y + r * 0.68f}, {center.x + r * 0.72f, center.y + r * 0.24f}, t, color);
            break;
        }
        case UiIcon::Package: {
            float t = std::max(1.4f, size * 0.1f);
            draw_stroke(window, {center.x, center.y - r * 0.74f}, {center.x + r * 0.68f, center.y - r * 0.34f}, t, color);
            draw_stroke(window, {center.x + r * 0.68f, center.y - r * 0.34f}, {center.x, center.y + r * 0.04f}, t, color);
            draw_stroke(window, {center.x, center.y + r * 0.04f}, {center.x - r * 0.68f, center.y - r * 0.34f}, t, color);
            draw_stroke(window, {center.x - r * 0.68f, center.y - r * 0.34f}, {center.x, center.y - r * 0.74f}, t, color);
            draw_stroke(window, {center.x - r * 0.68f, center.y - r * 0.34f}, {center.x - r * 0.68f, center.y + r * 0.4f}, t, color);
            draw_stroke(window, {center.x + r * 0.68f, center.y - r * 0.34f}, {center.x + r * 0.68f, center.y + r * 0.4f}, t, color);
            draw_stroke(window, {center.x, center.y + r * 0.04f}, {center.x, center.y + r * 0.76f}, t, color);
            draw_stroke(window, {center.x - r * 0.68f, center.y + r * 0.4f}, {center.x, center.y + r * 0.76f}, t, color);
            draw_stroke(window, {center.x + r * 0.68f, center.y + r * 0.4f}, {center.x, center.y + r * 0.76f}, t, color);
            break;
        }
        case UiIcon::Check: {
            float t = std::max(1.6f, size * 0.13f);
            draw_stroke(window, {center.x - r * 0.48f, center.y + r * 0.04f}, {center.x - r * 0.08f, center.y + r * 0.44f}, t, color);
            draw_stroke(window, {center.x - r * 0.08f, center.y + r * 0.44f}, {center.x + r * 0.52f, center.y - r * 0.38f}, t, color);
            break;
        }
        case UiIcon::ChevronUp: {
            float t = std::max(1.5f, size * 0.11f);
            draw_stroke(window, {center.x - r * 0.44f, center.y + r * 0.2f}, {center.x, center.y - r * 0.26f}, t, color);
            draw_stroke(window, {center.x, center.y - r * 0.26f}, {center.x + r * 0.44f, center.y + r * 0.2f}, t, color);
            break;
        }
        case UiIcon::ChevronDown: {
            float t = std::max(1.5f, size * 0.11f);
            draw_stroke(window, {center.x - r * 0.44f, center.y - r * 0.2f}, {center.x, center.y + r * 0.26f}, t, color);
            draw_stroke(window, {center.x, center.y + r * 0.26f}, {center.x + r * 0.44f, center.y - r * 0.2f}, t, color);
            break;
        }
        case UiIcon::ChevronRight: {
            float t = std::max(1.5f, size * 0.11f);
            draw_stroke(window, {center.x - r * 0.2f, center.y - r * 0.44f}, {center.x + r * 0.28f, center.y}, t, color);
            draw_stroke(window, {center.x + r * 0.28f, center.y}, {center.x - r * 0.2f, center.y + r * 0.44f}, t, color);
            break;
        }
        case UiIcon::Close: {
            float t = std::max(1.5f, size * 0.11f);
            draw_stroke(window, {center.x - r * 0.42f, center.y - r * 0.42f}, {center.x + r * 0.42f, center.y + r * 0.42f}, t, color);
            draw_stroke(window, {center.x + r * 0.42f, center.y - r * 0.42f}, {center.x - r * 0.42f, center.y + r * 0.42f}, t, color);
            break;
        }
        case UiIcon::Terminal: {
            float t = std::max(1.5f, size * 0.11f);
            draw_stroke(window, {center.x - r * 0.56f, center.y - r * 0.4f}, {center.x - r * 0.14f, center.y}, t, color);
            draw_stroke(window, {center.x - r * 0.14f, center.y}, {center.x - r * 0.56f, center.y + r * 0.4f}, t, color);
            draw_stroke(window, {center.x + r * 0.06f, center.y + r * 0.4f}, {center.x + r * 0.56f, center.y + r * 0.4f}, t, color);
            break;
        }
        case UiIcon::Plus: {
            float t = std::max(1.5f, size * 0.12f);
            draw_stroke(window, {center.x - r * 0.5f, center.y}, {center.x + r * 0.5f, center.y}, t, color);
            draw_stroke(window, {center.x, center.y - r * 0.5f}, {center.x, center.y + r * 0.5f}, t, color);
            break;
        }
        case UiIcon::Image: {
            float t = std::max(1.4f, size * 0.09f);
            sf::RectangleShape frame({size * 0.76f, size * 0.58f});
            frame.setPosition({center.x - r * 0.76f, center.y - r * 0.58f});
            frame.setFillColor(sf::Color::Transparent);
            frame.setOutlineColor(color);
            frame.setOutlineThickness(t);
            window.draw(frame);
            draw_stroke(window, {center.x - r * 0.52f, center.y + r * 0.34f}, {center.x - r * 0.14f, center.y - r * 0.04f}, t, color);
            draw_stroke(window, {center.x - r * 0.14f, center.y - r * 0.04f}, {center.x + r * 0.14f, center.y + r * 0.24f}, t, color);
            draw_stroke(window, {center.x + r * 0.14f, center.y + r * 0.24f}, {center.x + r * 0.52f, center.y - r * 0.14f}, t, color);
            break;
        }
        case UiIcon::Trophy: {
            float t = std::max(1.4f, size * 0.1f);
            draw_stroke(window, {center.x - r * 0.48f, center.y - r * 0.56f}, {center.x + r * 0.48f, center.y - r * 0.56f}, t, color);
            draw_stroke(window, {center.x - r * 0.48f, center.y - r * 0.56f}, {center.x - r * 0.3f, center.y + r * 0.08f}, t, color);
            draw_stroke(window, {center.x + r * 0.48f, center.y - r * 0.56f}, {center.x + r * 0.3f, center.y + r * 0.08f}, t, color);
            draw_stroke(window, {center.x - r * 0.3f, center.y + r * 0.08f}, {center.x + r * 0.3f, center.y + r * 0.08f}, t, color);
            draw_stroke(window, {center.x, center.y + r * 0.08f}, {center.x, center.y + r * 0.56f}, t, color);
            draw_stroke(window, {center.x - r * 0.38f, center.y + r * 0.56f}, {center.x + r * 0.38f, center.y + r * 0.56f}, t, color);
            break;
        }
    }
}

bool ui_button(
    UiContext& ui,
    sf::RenderWindow& window,
    sf::FloatRect rect,
    const std::string& label,
    sf::Color normal_bg,
    sf::Color hover_bg,
    sf::Color border_color,
    uint32_t font_size,
    float radius,
    UiIcon icon
) {
    uint64_t bid = hash_rect_id(rect, label);
    bool hovered = rect.contains(ui.mouse_pos);
    bool held = hovered && ui.mouse_down;
    float anim = ui_anim_hover(ui, bid, hovered);
    float press_t = std::max(held ? 1.f : 0.f, ui.press_anims[bid]);

    bool clicked = false;
    if (hovered && ui.mouse_clicked) {
        ui_consume_click(ui);
        ui.press_anims[bid] = 1.f;
        press_t = 1.f;
        clicked = true;
    }

    sf::FloatRect draw_rect = rect;
    if (press_t > 0.15f) {
        draw_rect.position.y += 1.f;
    }

    sf::Color bg = lerp_color(normal_bg, hover_bg, anim);
    if (press_t > 0.01f) {
        float lum_cur = (0.299f * bg.r + 0.587f * bg.g + 0.114f * bg.b) / 255.f;
        sf::Color press_target = lum_cur > 0.6f
            ? sf::Color(180, 180, 188, bg.a)
            : sf::Color(
                static_cast<uint8_t>(std::min(255, static_cast<int>(bg.r) + 28)),
                static_cast<uint8_t>(std::min(255, static_cast<int>(bg.g) + 28)),
                static_cast<uint8_t>(std::min(255, static_cast<int>(bg.b) + 32)),
                bg.a == 0 ? 45 : bg.a
            );
        bg = lerp_color(bg, press_target, press_t);
    }

    sf::Color br_target(
        static_cast<uint8_t>(std::min(255, static_cast<int>(border_color.r) + 55)),
        static_cast<uint8_t>(std::min(255, static_cast<int>(border_color.g) + 55)),
        static_cast<uint8_t>(std::min(255, static_cast<int>(border_color.b) + 55)),
        border_color.a == 0 ? 0 : std::max<uint8_t>(border_color.a, 145)
    );
    sf::Color br = lerp_color(border_color, br_target, std::max(anim, press_t));

    ui_draw_rounded_panel(window, draw_rect, std::min(radius, 3.f), bg, br, br.a > 0 ? 1.f : 0.f, 0);

    float lum = (0.299f * bg.r + 0.587f * bg.g + 0.114f * bg.b) / 255.f;
    sf::Color fg = (lum > 0.62f && bg.a > 160) ? sf::Color(10, 10, 12) : sf::Color(244, 244, 245);

    if (icon != UiIcon::None && !label.empty()) {
        float icon_sz = static_cast<float>(font_size) * 0.85f;
        float gap = 8.f;
        const std::string& fitted = ui_cached_truncate(ui, label, font_size, std::max(12.f, draw_rect.size.x - icon_sz - gap - 16.f));
        UiCachedText& c = ui_get_cached_text(ui, fitted, font_size);
        c.text.setFillColor(fg);
        float total_w = icon_sz + gap + c.bounds.size.x;
        float start_x = draw_rect.position.x + (draw_rect.size.x - total_w) * 0.5f;
        float cy = draw_rect.position.y + draw_rect.size.y * 0.5f;
        ui_draw_icon(window, icon, {start_x + icon_sz * 0.5f, cy}, icon_sz, fg);
        c.text.setPosition({std::round(start_x + icon_sz + gap - c.bounds.position.x), std::round(cy - c.bounds.size.y * 0.5f - c.bounds.position.y)});
        window.draw(c.text);
    } else if (icon != UiIcon::None) {
        ui_draw_icon(window, icon, {draw_rect.position.x + draw_rect.size.x * 0.5f, draw_rect.position.y + draw_rect.size.y * 0.5f}, static_cast<float>(font_size), fg);
    } else {
        ui_draw_label_centered(ui, window, label, draw_rect, font_size, fg);
    }

    return clicked;
}

bool ui_nav_item(
    UiContext& ui,
    sf::RenderWindow& window,
    sf::FloatRect rect,
    const std::string& label,
    UiIcon icon,
    bool active,
    sf::Color accent_color
) {
    bool hovered = rect.contains(ui.mouse_pos);
    float anim = ui_anim_hover(ui, hash_rect_id(rect, "nav_" + label), hovered);

    sf::Color bg = active
        ? sf::Color(24, 24, 28)
        : lerp_color(sf::Color(0, 0, 0, 0), sf::Color(18, 18, 22), anim);
    sf::Color br = active ? sf::Color(44, 44, 52) : sf::Color(0, 0, 0, 0);

    if (bg.a > 0) {
        ui_draw_rounded_panel(window, rect, 3.f, bg, br, active ? 1.f : 0.f, 0);
    }

    if (active) {
        sf::RectangleShape bar({2.f, rect.size.y - 14.f});
        bar.setPosition({rect.position.x, rect.position.y + 7.f});
        bar.setFillColor(accent_color);
        window.draw(bar);
    }

    sf::Color fg = active ? sf::Color(250, 250, 250) : lerp_color(sf::Color(138, 138, 147), sf::Color(232, 232, 236), anim);
    ui_draw_icon(window, icon, {rect.position.x + 20.f, rect.position.y + rect.size.y * 0.5f}, 14.f, fg);
    ui_draw_label_clamped(ui, window, label, {rect.position.x + 38.f, rect.position.y + (rect.size.y - 16.f) * 0.5f - 1.f}, 14, fg, rect.size.x - 46.f);

    if (hovered && ui.mouse_clicked) {
        ui_consume_click(ui);
        return true;
    }
    return false;
}

bool ui_toggle(
    UiContext& ui,
    sf::RenderWindow& window,
    sf::Vector2f pos,
    const std::string& label,
    bool value,
    sf::Color accent_color
) {
    const float w = 36.f;
    const float h = 18.f;
    float label_w = label.empty() ? 0.f : (ui_cached_width(ui, label, 14) + 14.f);
    sf::FloatRect hit({pos.x, pos.y - 2.f}, {w + label_w, h + 4.f});
    bool hovered = hit.contains(ui.mouse_pos);
    float anim = ui_anim_hover(ui, hash_rect_id({pos, {w, h}}, "tog_" + label), value);

    sf::Color track_bg = lerp_color(sf::Color(24, 24, 28), accent_color, anim);
    sf::Color track_border = hovered ? sf::Color(113, 113, 122) : sf::Color(48, 48, 56);
    ui_draw_rounded_panel(window, sf::FloatRect({std::round(pos.x), std::round(pos.y)}, {w, h}), 2.f, track_bg, track_border, 1.f, 0);

    float knob_sz = 12.f;
    float knob_x = std::round(pos.x + 3.f + anim * (w - knob_sz - 6.f));
    sf::RectangleShape knob({knob_sz, knob_sz});
    knob.setPosition({knob_x, std::round(pos.y + 3.f)});
    float lum = (0.299f * track_bg.r + 0.587f * track_bg.g + 0.114f * track_bg.b) / 255.f;
    knob.setFillColor(lum > 0.65f ? sf::Color(12, 12, 14) : sf::Color(244, 244, 245));
    window.draw(knob);

    if (!label.empty()) {
        ui_draw_label(ui, window, label, {pos.x + w + 12.f, pos.y}, 14, sf::Color(212, 212, 216));
    }
    if (hovered && ui.mouse_clicked) {
        ui_consume_click(ui);
        return true;
    }
    return false;
}

bool ui_checkbox(
    UiContext& ui,
    sf::RenderWindow& window,
    sf::Vector2f pos,
    float box_size,
    const std::string& label,
    bool checked
) {
    float label_w = label.empty() ? 0.f : (ui_cached_width(ui, label, 14) + 12.f);
    sf::FloatRect hit({pos.x, pos.y}, {box_size + label_w, box_size});
    bool hovered = hit.contains(ui.mouse_pos);
    sf::FloatRect box_rect({std::round(pos.x), std::round(pos.y)}, {box_size, box_size});
    float anim = ui_anim_hover(ui, hash_rect_id(box_rect, "chk_" + label), hovered);

    sf::Color fill = checked
        ? sf::Color(244, 244, 245)
        : lerp_color(sf::Color(16, 16, 20), sf::Color(28, 28, 34), anim);
    sf::Color border = checked
        ? sf::Color(244, 244, 245)
        : lerp_color(sf::Color(54, 54, 62), sf::Color(113, 113, 122), anim);

    ui_draw_rounded_panel(window, box_rect, 2.f, fill, border, 1.f, 0);

    if (checked) {
        ui_draw_icon(window, UiIcon::Check, {pos.x + box_size * 0.5f, pos.y + box_size * 0.5f}, box_size * 0.62f, sf::Color(10, 10, 12));
    }

    if (!label.empty()) {
        ui_draw_label(ui, window, label, {pos.x + box_size + 10.f, pos.y + (box_size - 16.f) * 0.5f - 1.f}, 14, sf::Color(212, 212, 216));
    }
    if (hovered && ui.mouse_clicked) {
        ui_consume_click(ui);
        return true;
    }
    return false;
}

bool ui_slider(
    UiContext& ui,
    sf::RenderWindow& window,
    int slider_id,
    sf::FloatRect track_rect,
    float& value,
    float min_val,
    float max_val,
    sf::Color accent_color
) {
    sf::FloatRect hit(
        {track_rect.position.x - 10.f, track_rect.position.y - 12.f},
        {track_rect.size.x + 20.f, track_rect.size.y + 24.f}
    );
    bool hovered = hit.contains(ui.mouse_pos);
    bool changed = false;

    if (hovered && ui.mouse_clicked) {
        ui.active_slider_id = slider_id;
        ui_consume_click(ui);
    }
    if (ui.active_slider_id == slider_id && ui.mouse_down) {
        float t = (ui.mouse_pos.x - track_rect.position.x) / std::max(1.f, track_rect.size.x);
        t = std::clamp(t, 0.f, 1.f);
        float next_val = min_val + (max_val - min_val) * t;
        if (std::abs(next_val - value) > 0.01f) {
            value = next_val;
            changed = true;
        }
    }

    float norm = std::clamp((value - min_val) / std::max(0.0001f, max_val - min_val), 0.f, 1.f);
    float bar_h = 4.f;
    float bar_x = std::round(track_rect.position.x);
    float bar_y = std::round(track_rect.position.y + (track_rect.size.y - bar_h) * 0.5f);
    float bar_w = std::round(track_rect.size.x);
    float fill_w = std::round(bar_w * norm);

    sf::RectangleShape track({bar_w, bar_h});
    track.setPosition({bar_x, bar_y});
    track.setFillColor(sf::Color(36, 36, 42));
    window.draw(track);

    sf::RectangleShape fill({fill_w, bar_h});
    fill.setPosition({bar_x, bar_y});
    fill.setFillColor(accent_color);
    window.draw(fill);

    float kw = 8.f;
    float kh = 14.f;
    float kx = std::round(bar_x + fill_w - kw * 0.5f);
    float ky = std::round(bar_y + bar_h * 0.5f - kh * 0.5f);
    sf::RectangleShape knob({kw, kh});
    knob.setPosition({kx, ky});
    knob.setFillColor(sf::Color(244, 244, 245));
    knob.setOutlineColor(sf::Color(10, 10, 12));
    knob.setOutlineThickness(1.f);
    window.draw(knob);

    return changed;
}

void ui_text_input(
    UiContext& ui,
    sf::RenderWindow& window,
    int input_id,
    sf::FloatRect rect,
    std::string& buffer,
    const std::string& placeholder,
    sf::Color accent_color
) {
    bool hovered = rect.contains(ui.mouse_pos);
    if (ui.mouse_clicked) {
        if (hovered) {
            ui.focused_input_id = input_id;
            ui.blink_timer = 0.f;
            ui_consume_click(ui);
        } else if (ui.focused_input_id == input_id) {
            ui.focused_input_id = -1;
        }
    }

    bool focused = (ui.focused_input_id == input_id);
    if (focused) {
        if (ui.backspace_pressed && !buffer.empty()) {
            pop_utf8_char(buffer);
            ui.blink_timer = 0.f;
        }
        if (!ui.text_entered_utf8.empty() && buffer.size() < 96) {
            buffer += ui.text_entered_utf8;
            ui.blink_timer = 0.f;
        }
    }

    sf::Color border = focused ? accent_color : (hovered ? sf::Color(82, 82, 91) : sf::Color(40, 40, 46));
    ui_draw_rounded_panel(window, rect, 3.f, sf::Color(10, 10, 12), border, 1.f, 0);

    float text_x = rect.position.x + 12.f;
    float text_y = rect.position.y + (rect.size.y - 16.f) * 0.5f - 1.f;
    float max_w = std::max(16.f, rect.size.x - 24.f);

    if (buffer.empty()) {
        ui_draw_label_clamped(ui, window, placeholder, {text_x, text_y}, 14, sf::Color(82, 82, 91), max_w);
    } else {
        ui_draw_label_clamped(ui, window, buffer, {text_x, text_y}, 14, sf::Color(244, 244, 245), max_w);
    }

    if (focused && (static_cast<int>(ui.blink_timer * 2.f) % 2 == 0)) {
        float caret_x = text_x;
        if (!buffer.empty()) {
            const std::string& shown = ui_cached_truncate(ui, buffer, 14, max_w);
            caret_x += ui_cached_width(ui, shown, 14) + 2.f;
        }
        caret_x = std::min(caret_x, rect.position.x + rect.size.x - 10.f);
        sf::RectangleShape caret({1.5f, 16.f});
        caret.setPosition({std::round(caret_x), std::round(rect.position.y + (rect.size.y - 16.f) * 0.5f)});
        caret.setFillColor(sf::Color(244, 244, 245));
        window.draw(caret);
    }
}

void ui_draw_scrollbar(
    sf::RenderWindow& window,
    sf::FloatRect track_rect,
    int total_items,
    int visible_items,
    int scroll_offset,
    sf::Color accent_color
) {
    if (total_items <= visible_items || visible_items <= 0) return;
    sf::RectangleShape track(track_rect.size);
    track.setPosition(track_rect.position);
    track.setFillColor(sf::Color(26, 26, 30));
    window.draw(track);

    float ratio = static_cast<float>(visible_items) / static_cast<float>(total_items);
    float thumb_h = std::max(24.f, track_rect.size.y * ratio);
    int max_scroll = std::max(1, total_items - visible_items);
    float pos_ratio = static_cast<float>(std::clamp(scroll_offset, 0, max_scroll)) / static_cast<float>(max_scroll);
    float thumb_y = track_rect.position.y + (track_rect.size.y - thumb_h) * pos_ratio;

    sf::RectangleShape thumb({track_rect.size.x, thumb_h});
    thumb.setPosition({track_rect.position.x, std::round(thumb_y)});
    thumb.setFillColor(accent_color);
    window.draw(thumb);
}

bool ui_pagination(
    UiContext& ui,
    sf::RenderWindow& window,
    sf::Vector2f center_pos,
    int total_pages,
    int& current_page,
    sf::Color accent_color
) {
    if (total_pages <= 1) return false;
    float btn_w = 32.f;
    float btn_h = 28.f;
    float gap = 6.f;
    float total_w = static_cast<float>(total_pages) * btn_w + static_cast<float>(total_pages - 1) * gap;
    float start_x = center_pos.x - total_w * 0.5f;
    bool changed = false;

    for (int p = 0; p < total_pages; ++p) {
        bool active = (p == current_page);
        sf::FloatRect r({start_x + static_cast<float>(p) * (btn_w + gap), center_pos.y}, {btn_w, btn_h});
        sf::Color bg = active ? accent_color : sf::Color(18, 18, 22);
        sf::Color hov = active ? accent_color : sf::Color(30, 30, 36);
        sf::Color br = active ? accent_color : sf::Color(44, 44, 52);
        if (ui_button(ui, window, r, std::to_string(p + 1), bg, hov, br, 13, 3.f)) {
            current_page = p;
            changed = true;
        }
    }
    return changed;
}
