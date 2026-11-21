#pragma once
#include <SFML/Graphics.hpp>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

enum class UiIcon {
    None = 0,
    Play,
    Code,
    Sliders,
    Folder,
    Layers,
    Package,
    Check,
    ChevronUp,
    ChevronDown,
    ChevronRight,
    Close,
    Terminal,
    Plus,
    Image,
    Trophy
};

struct UiCachedText {
    sf::Text text;
    sf::FloatRect bounds;
};

struct UiContext {
    sf::Font font;
    std::vector<uint8_t> font_buffer;
    sf::Vector2f mouse_pos = {0.f, 0.f};
    bool mouse_down = false;
    bool mouse_clicked = false;
    bool mouse_released = false;
    float wheel_delta = 0.f;
    float dt = 0.016f;
    float blink_timer = 0.f;
    int active_slider_id = -1;
    int focused_input_id = -1;
    std::string text_entered_utf8;
    bool backspace_pressed = false;
    std::unordered_map<uint64_t, float> hover_anims;
    std::unordered_map<uint64_t, float> press_anims;
    std::unordered_map<uint64_t, UiCachedText> text_cache;
    std::unordered_map<uint64_t, std::string> trunc_cache;
};

bool ui_init(UiContext& ui, const std::string& font_path);
void ui_reset_frame(UiContext& ui, float dt = 0.016f);
void ui_consume_click(UiContext& ui);
void ui_handle_event(UiContext& ui, const sf::Event& event, const sf::RenderWindow& window);
float ui_anim_hover(UiContext& ui, uint64_t id, bool hovered);

float ui_measure_text_width(const sf::Font& font, const std::string& text, uint32_t size);
std::string ui_truncate_text(const sf::Font& font, const std::string& text, uint32_t size, float max_width);
void ui_draw_divider(sf::RenderWindow& window, sf::Vector2f pos, float width, sf::Color color);

sf::ConvexShape ui_make_rounded_rect(sf::FloatRect rect, float radius, int corner_pts);
void ui_draw_gradient_rect(sf::RenderWindow& window, sf::FloatRect rect, sf::Color top_color, sf::Color bottom_color);
void ui_draw_horizontal_gradient(sf::RenderWindow& window, sf::FloatRect rect, sf::Color left_color, sf::Color right_color);
void ui_draw_rounded_panel(
    sf::RenderWindow& window,
    sf::FloatRect rect,
    float radius,
    sf::Color fill,
    sf::Color outline,
    float thickness,
    uint8_t shadow_alpha
);
void ui_draw_glass_panel(
    sf::RenderWindow& window,
    sf::FloatRect rect,
    float radius,
    sf::Color fill,
    sf::Color outline,
    uint8_t shadow_alpha
);
void ui_draw_panel(sf::RenderWindow& window, sf::FloatRect rect, sf::Color fill, sf::Color outline, float thickness);
void ui_draw_label(UiContext& ui, sf::RenderWindow& window, const std::string& text, sf::Vector2f pos, uint32_t size, sf::Color color);
void ui_draw_label_clamped(UiContext& ui, sf::RenderWindow& window, const std::string& text, sf::Vector2f pos, uint32_t size, sf::Color color, float max_width);
void ui_draw_label_centered(UiContext& ui, sf::RenderWindow& window, const std::string& text, sf::FloatRect rect, uint32_t size, sf::Color color);
void ui_draw_soft_badge(UiContext& ui, sf::RenderWindow& window, sf::FloatRect rect, const std::string& text, sf::Color accent, uint32_t font_size);
void ui_draw_icon(sf::RenderWindow& window, UiIcon icon, sf::Vector2f center, float size, sf::Color color);

bool ui_button(
    UiContext& ui,
    sf::RenderWindow& window,
    sf::FloatRect rect,
    const std::string& label,
    sf::Color normal_bg,
    sf::Color hover_bg,
    sf::Color border_color,
    uint32_t font_size,
    float radius = 3.f,
    UiIcon icon = UiIcon::None
);

bool ui_nav_item(
    UiContext& ui,
    sf::RenderWindow& window,
    sf::FloatRect rect,
    const std::string& label,
    UiIcon icon,
    bool active,
    sf::Color accent_color
);

bool ui_toggle(
    UiContext& ui,
    sf::RenderWindow& window,
    sf::Vector2f pos,
    const std::string& label,
    bool value,
    sf::Color accent_color
);

bool ui_checkbox(
    UiContext& ui,
    sf::RenderWindow& window,
    sf::Vector2f pos,
    float box_size,
    const std::string& label,
    bool checked
);

bool ui_slider(
    UiContext& ui,
    sf::RenderWindow& window,
    int slider_id,
    sf::FloatRect track_rect,
    float& value,
    float min_val,
    float max_val,
    sf::Color accent_color
);

void ui_text_input(
    UiContext& ui,
    sf::RenderWindow& window,
    int input_id,
    sf::FloatRect rect,
    std::string& buffer,
    const std::string& placeholder,
    sf::Color accent_color
);

void ui_draw_scrollbar(
    sf::RenderWindow& window,
    sf::FloatRect track_rect,
    int total_items,
    int visible_items,
    int scroll_offset,
    sf::Color accent_color
);

bool ui_pagination(
    UiContext& ui,
    sf::RenderWindow& window,
    sf::Vector2f center_pos,
    int total_pages,
    int& current_page,
    sf::Color accent_color
);