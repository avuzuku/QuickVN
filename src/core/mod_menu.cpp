#include "core/mod_menu.hpp"
#include <algorithm>
#include <utility>

static constexpr float MOD_LOGICAL_W = 1280.f;
static constexpr float MOD_LOGICAL_H = 720.f;

static sf::Color mod_sf_color(QvnColor c) {
    return sf::Color(c.r, c.g, c.b, c.a);
}

static std::string mod_loc(const std::string& locale_json, const char* key, const char* fallback) {
    if (!locale_json.empty()) {
        char* val = qvn_locale_lookup(locale_json.c_str(), key);
        if (val) {
            std::string res(val);
            qvn_free_cstr(val);
            return res;
        }
    }
    return fallback ? std::string(fallback) : std::string(key);
}

void mod_menu_reload(ModMenuState& menu, const std::string& base_dir) {
    menu.items.clear();
    QvnModList list = {};
    if (qvn_mods_scan(base_dir.c_str(), &list)) {
        for (size_t i = 0; i < list.count; ++i) {
            menu.items.push_back(list.items[i]);
        }
        qvn_mods_free(&list);
    }
    menu.scroll = 0;
    menu.dirty = false;
}

bool mod_menu_save(ModMenuState& menu, const std::string& base_dir) {
    int ok = qvn_mods_save(
        base_dir.c_str(),
        menu.items.empty() ? nullptr : menu.items.data(),
        menu.items.size()
    );
    if (ok) {
        menu.dirty = false;
    }
    return ok != 0;
}

ModMenuAction mod_menu_render(
    ModMenuState& menu,
    UiContext& ui,
    sf::RenderWindow& window,
    const QvnConfig& cfg,
    const std::string& locale_json,
    const std::string& base_dir
) {
    sf::RectangleShape dim({MOD_LOGICAL_W, MOD_LOGICAL_H});
    dim.setFillColor(sf::Color(8, 8, 10, 210));
    window.draw(dim);

    float pw = 880.f;
    float ph = 590.f;
    float px = (MOD_LOGICAL_W - pw) * 0.5f;
    float py = (MOD_LOGICAL_H - ph) * 0.5f;

    sf::Color acc = mod_sf_color(cfg.theme_accent_color);
    sf::Color acc_hov = mod_sf_color(cfg.theme_accent_hover_color);
    sf::Color panel_bg(12, 12, 14, 248);
    sf::Color panel_br(42, 42, 48);
    sf::Color btn_bg(20, 20, 24);
    sf::Color btn_hov(32, 32, 38);

    ui_draw_rounded_panel(window, sf::FloatRect({px, py}, {pw, ph}), 4.f, panel_bg, panel_br, 1.f, 0);

    size_t enabled_count = 0;
    for (const auto& m : menu.items) {
        if (m.enabled) enabled_count++;
    }

    std::string header = mod_loc(locale_json, "mods_title", "Mod Manager");
    ui_draw_label(ui, window, header, {px + 28.f, py + 20.f}, 20, sf::Color(244, 244, 245));

    float title_w = ui_measure_text_width(ui.font, header, 20);
    std::string cnt_str = std::to_string(enabled_count) + " / " + std::to_string(menu.items.size());
    float badge_w = std::max(54.f, ui_measure_text_width(ui.font, cnt_str, 12) + 20.f);
    ui_draw_soft_badge(ui, window, sf::FloatRect({px + 28.f + title_w + 14.f, py + 21.f}, {badge_w, 22.f}), cnt_str, acc, 12);

    ui_draw_label_clamped(ui, window, mod_loc(locale_json, "mods_subtitle", "Toggle mods and arrange load order inside the game"), {px + 28.f, py + 48.f}, 13, sf::Color(138, 138, 147), pw - 110.f);

    if (ui_button(ui, window, sf::FloatRect({px + pw - 58.f, py + 18.f}, {32.f, 32.f}), "", sf::Color(20, 20, 24), sf::Color(34, 34, 40), panel_br, 13, 3.f, UiIcon::Close)) {
        return ModMenuAction::Close;
    }

    ui_draw_divider(window, {px, py + 76.f}, pw, sf::Color(32, 32, 38));

    float list_x = px + 24.f;
    float list_y = py + 90.f;
    float list_w = pw - 48.f;
    float list_h = ph - 166.f;

    ui_draw_rounded_panel(window, sf::FloatRect({list_x, list_y}, {list_w, list_h}), 3.f, sf::Color(9, 9, 11), sf::Color(32, 32, 38), 1.f, 0);

    const int visible_rows = 5;
    int max_scroll = std::max(0, static_cast<int>(menu.items.size()) - visible_rows);
    if (ui.wheel_delta != 0.f) {
        menu.scroll = std::clamp(menu.scroll - static_cast<int>(ui.wheel_delta), 0, max_scroll);
    }

    if (menu.items.empty()) {
        ui_draw_icon(window, UiIcon::Layers, {list_x + list_w * 0.5f, list_y + list_h * 0.4f}, 28.f, sf::Color(72, 72, 80));
        ui_draw_label_centered(ui, window, mod_loc(locale_json, "mods_empty", "No mods installed"), sf::FloatRect({list_x, list_y + list_h * 0.5f}, {list_w, 24.f}), 16, sf::Color(212, 212, 216));
        ui_draw_label_centered(ui, window, mod_loc(locale_json, "mods_empty_hint", "Place mod folders inside ./mods/ or ./engine_mods/"), sf::FloatRect({list_x, list_y + list_h * 0.58f}, {list_w, 22.f}), 13, sf::Color(113, 113, 122));
    } else {
        const float row_h = 72.f;
        const float row_gap = 8.f;
        float row_w = max_scroll > 0 ? (list_w - 34.f) : (list_w - 20.f);

        for (int r = 0; r < visible_rows; ++r) {
            size_t idx = static_cast<size_t>(menu.scroll + r);
            if (idx >= menu.items.size()) break;

            QvnModInfo& m = menu.items[idx];
            float ry = list_y + 10.f + static_cast<float>(r) * (row_h + row_gap);
            sf::Color row_br = m.enabled ? acc : sf::Color(34, 34, 40);

            ui_draw_rounded_panel(window, sf::FloatRect({list_x + 10.f, ry}, {row_w, row_h}), 3.f, sf::Color(15, 15, 18), row_br, 1.f, 0);

            if (ui_checkbox(ui, window, {list_x + 24.f, ry + 26.f}, 18.f, "", m.enabled != 0)) {
                m.enabled = m.enabled ? 0 : 1;
                menu.dirty = true;
            }

            sf::Color badge_col = m.is_engine_mod ? sf::Color(168, 85, 247) : sf::Color(16, 185, 129);
            std::string scope_lbl = m.is_engine_mod
                ? mod_loc(locale_json, "mods_engine", "ENGINE")
                : mod_loc(locale_json, "mods_game", "GAME");
            float scope_w = std::max(64.f, ui_measure_text_width(ui.font, scope_lbl, 11) + 16.f);
            ui_draw_soft_badge(ui, window, sf::FloatRect({list_x + 56.f, ry + 13.f}, {scope_w, 20.f}), scope_lbl, badge_col, 11);

            float title_x = list_x + 56.f + scope_w + 12.f;
            float right_limit = list_x + 10.f + row_w - 92.f;
            std::string title = std::string(m.name) + "  v" + std::string(m.version) + "  —  " + std::string(m.author);
            ui_draw_label_clamped(ui, window, title, {title_x, ry + 14.f}, 14, sf::Color(244, 244, 245), std::max(40.f, right_limit - title_x));

            std::string desc = m.description[0] ? std::string(m.description) : std::string(m.id);
            ui_draw_label_clamped(ui, window, desc, {list_x + 56.f, ry + 43.f}, 12, sf::Color(138, 138, 147), std::max(40.f, right_limit - (list_x + 56.f)));

            float btn_up_x = list_x + 10.f + row_w - 78.f;
            float btn_dn_x = list_x + 10.f + row_w - 40.f;

            if (ui_button(ui, window, sf::FloatRect({btn_up_x, ry + 20.f}, {32.f, 32.f}), "", btn_bg, btn_hov, panel_br, 12, 3.f, UiIcon::ChevronUp)) {
                if (idx > 0) {
                    std::swap(menu.items[idx], menu.items[idx - 1]);
                    menu.dirty = true;
                }
            }
            if (ui_button(ui, window, sf::FloatRect({btn_dn_x, ry + 20.f}, {32.f, 32.f}), "", btn_bg, btn_hov, panel_br, 12, 3.f, UiIcon::ChevronDown)) {
                if (idx + 1 < menu.items.size()) {
                    std::swap(menu.items[idx], menu.items[idx + 1]);
                    menu.dirty = true;
                }
            }
        }

        if (max_scroll > 0) {
            ui_draw_scrollbar(
                window,
                sf::FloatRect({list_x + list_w - 10.f, list_y + 10.f}, {4.f, list_h - 20.f}),
                static_cast<int>(menu.items.size()),
                visible_rows,
                menu.scroll,
                acc
            );
        }
    }

    float foot_y = py + ph - 54.f;
    if (ui_button(ui, window, sf::FloatRect({px + 24.f, foot_y}, {126.f, 34.f}), mod_loc(locale_json, "mods_rescan", "Rescan"), btn_bg, btn_hov, panel_br, 13, 3.f)) {
        mod_menu_reload(menu, base_dir);
    }
    if (ui_button(ui, window, sf::FloatRect({px + 158.f, foot_y}, {176.f, 34.f}), mod_loc(locale_json, "mods_create_demo", "Create Test Mod"), btn_bg, btn_hov, panel_br, 13, 3.f, UiIcon::Plus)) {
        if (qvn_create_test_mod(base_dir.c_str())) {
            mod_menu_reload(menu, base_dir);
            return ModMenuAction::TestModCreated;
        }
    }
    if (ui_button(ui, window, sf::FloatRect({px + pw - 216.f, foot_y}, {192.f, 34.f}), mod_loc(locale_json, "mods_apply", "Apply & Reload"), acc, acc_hov, acc, 14, 3.f, UiIcon::Check)) {
        return ModMenuAction::ApplyAndReload;
    }

    return ModMenuAction::None;
}