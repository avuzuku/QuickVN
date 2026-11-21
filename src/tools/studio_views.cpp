#include "tools/studio_state.hpp"
#include "platform/platform.hpp"
#include <algorithm>
#include <cstdio>
#include <filesystem>

static bool draw_tile(UiContext& ui, sf::RenderWindow& window, sf::FloatRect rect, UiIcon icon, const std::string& title, const std::string& subtitle) {
    bool hovered = rect.contains(ui.mouse_pos);
    float anim = ui_anim_hover(ui, static_cast<uint64_t>(rect.position.x * 17.f + rect.position.y * 29.f), hovered);

    ui_draw_rounded_panel(window, rect, 3.f, hovered ? COL_SURFACE_HOV : COL_SURFACE, hovered ? COL_BORDER_HI : COL_BORDER, 1.f, 0);
    ui_draw_icon(window, icon, {rect.position.x + 20.f, rect.position.y + rect.size.y * 0.5f}, 14.f, hovered ? COL_FG : COL_MUTED);

    float tx = rect.position.x + 38.f;
    float tw = std::max(24.f, rect.size.x - 48.f);
    ui_draw_label_clamped(ui, window, title, {tx, rect.position.y + 11.f}, 13, COL_FG, tw);
    ui_draw_label_clamped(ui, window, subtitle, {tx, rect.position.y + 31.f}, 11, anim > 0.5f ? COL_MUTED : COL_DIM, tw);

    if (hovered && ui.mouse_clicked) {
        ui_consume_click(ui);
        return true;
    }
    return false;
}

static std::string format_bytes(uint64_t bytes) {
    char buf[32];
    if (bytes >= 1024 * 1024) {
        std::snprintf(buf, sizeof(buf), "%.1f MB", static_cast<double>(bytes) / (1024.0 * 1024.0));
    } else if (bytes >= 1024) {
        std::snprintf(buf, sizeof(buf), "%.1f KB", static_cast<double>(bytes) / 1024.0);
    } else {
        std::snprintf(buf, sizeof(buf), "%llu B", static_cast<unsigned long long>(bytes));
    }
    return std::string(buf);
}

static void render_console(LauncherState& st, UiContext& ui, sf::RenderWindow& window, sf::FloatRect rect, const std::string& title) {
    ui_draw_rounded_panel(window, rect, 3.f, COL_SURFACE, COL_BORDER, 1.f, 0);
    ui_draw_label(ui, window, title, {rect.position.x + 14.f, rect.position.y + 10.f}, 12, COL_MUTED);

    if (ui_button(ui, window, sf::FloatRect({rect.position.x + rect.size.x - 68.f, rect.position.y + 6.f}, {58.f, 22.f}), studio_tr(st.lang_idx, "Clear", "Очист.", "消去"), COL_BG, COL_SURFACE_HOV, COL_BORDER, 11, 2.f)) {
        std::lock_guard<std::mutex> lock(st.log_mutex);
        st.logs.clear();
    }

    ui_draw_divider(window, {rect.position.x, rect.position.y + 34.f}, rect.size.x, COL_BORDER);

    float inner_y = rect.position.y + 42.f;
    float inner_h = std::max(24.f, rect.size.y - 48.f);
    float max_w = std::max(40.f, rect.size.x - 28.f);

    std::lock_guard<std::mutex> lock(st.log_mutex);
    size_t max_lines = static_cast<size_t>(std::max(1, static_cast<int>(inner_h / 19.f)));
    size_t start = st.logs.size() > max_lines ? st.logs.size() - max_lines : 0;
    for (size_t i = start; i < st.logs.size(); ++i) {
        const std::string& line = st.logs[i];
        sf::Color col = COL_MUTED;
        if (line.find("Error") != std::string::npos || line.find("failed") != std::string::npos || line.find("Missing") != std::string::npos) col = COL_ERR;
        else if (line.find("Warning") != std::string::npos) col = COL_WARN;
        else if (line.find("Packed") != std::string::npos || line.find("Exported") != std::string::npos || line.find("Created") != std::string::npos || line.find("passed") != std::string::npos || line.find("Launched") != std::string::npos || line.find("Saved") != std::string::npos) col = COL_OK;
        ui_draw_label_clamped(ui, window, line, {rect.position.x + 14.f, inner_y + static_cast<float>(i - start) * 19.f}, 12, col, max_w);
    }
}

void studio_render_workspace(LauncherState& st, UiContext& ui, sf::RenderWindow& window, float cx, float cy, float cw, float ch) {
    int l = st.lang_idx;
    ui_draw_rounded_panel(window, sf::FloatRect({cx, cy}, {cw, 70.f}), 3.f, COL_SURFACE, COL_BORDER, 1.f, 0);

    float text_max_w = std::max(80.f, cw - 320.f);
    std::string p_title = st.active_project_name.empty() ? studio_tr(l, "No Active Project", "Нет активного проекта", "プロジェクト未選択") : st.active_project_name;
    std::string p_path = st.active_project_dir.empty() ? st.proj_root : st.active_project_dir;

    ui_draw_label_clamped(ui, window, p_title, {cx + 16.f, cy + 12.f}, 19, COL_FG, text_max_w);
    ui_draw_label_clamped(ui, window, p_path, {cx + 16.f, cy + 41.f}, 12, COL_DIM, text_max_w);

    float bx = cx + cw - 14.f;
    bx -= 98.f;
    if (ui_button(ui, window, sf::FloatRect({bx, cy + 18.f}, {98.f, 34.f}), studio_tr(l, "Run", "Запуск", "実行"), COL_PRIMARY, COL_PRIMARY_HOV, COL_PRIMARY, 13, 3.f, UiIcon::Play)) {
        studio_launch_game(st);
    }
    bx -= 88.f;
    if (ui_button(ui, window, sf::FloatRect({bx, cy + 18.f}, {80.f, 34.f}), studio_tr(l, "Pack", "Пак", "パック"), COL_BG, COL_SURFACE_HOV, COL_BORDER, 13, 3.f, UiIcon::Package)) {
        studio_start_quick_pack(st);
    }
    bx -= 92.f;
    if (ui_button(ui, window, sf::FloatRect({bx, cy + 18.f}, {84.f, 34.f}), studio_tr(l, "Check", "Чек", "検証"), COL_BG, COL_SURFACE_HOV, COL_BORDER, 13, 3.f, UiIcon::Check)) {
        studio_validate_project(st);
    }

    float g_y = cy + 84.f;
    float gap = 10.f;
    float tw = (cw - gap * 3.f) / 4.f;
    float th = 56.f;
    std::filesystem::path pp(st.active_project_dir);

    if (draw_tile(ui, window, sf::FloatRect({cx, g_y}, {tw, th}), UiIcon::Code, studio_tr(l, "Main Script", "Скрипт", "スクリプト"), "scripts/main.js")) {
        if (!st.active_project_dir.empty()) platform_open_in_editor((pp / "scripts/main.js").string(), st.cfg.editor_command);
    }
    if (draw_tile(ui, window, sf::FloatRect({cx + (tw + gap), g_y}, {tw, th}), UiIcon::Sliders, studio_tr(l, "Config", "Конфиг", "設定"), "config.json")) {
        if (!st.active_project_dir.empty()) platform_open_in_editor((pp / "config.json").string(), st.cfg.editor_command);
    }
    if (draw_tile(ui, window, sf::FloatRect({cx + (tw + gap) * 2.f, g_y}, {tw, th}), UiIcon::Image, studio_tr(l, "Backgrounds", "Фоны", "背景"), "assets/bg/")) {
        if (!st.active_project_dir.empty()) platform_open_folder((pp / "assets/bg").string());
    }
    if (draw_tile(ui, window, sf::FloatRect({cx + (tw + gap) * 3.f, g_y}, {tw, th}), UiIcon::Image, studio_tr(l, "Characters", "Персонажи", "立ち絵"), "assets/chars/")) {
        if (!st.active_project_dir.empty()) platform_open_folder((pp / "assets/chars").string());
    }

    float r2 = g_y + th + gap;
    if (draw_tile(ui, window, sf::FloatRect({cx, r2}, {tw, th}), UiIcon::Folder, studio_tr(l, "Music (BGM)", "Музыка", "BGM"), "assets/audio/music/")) {
        if (!st.active_project_dir.empty()) platform_open_folder((pp / "assets/audio/music").string());
    }
    if (draw_tile(ui, window, sf::FloatRect({cx + (tw + gap), r2}, {tw, th}), UiIcon::Folder, studio_tr(l, "SFX & Voice", "Звуки и голос", "SE・ボイス"), "assets/audio/")) {
        if (!st.active_project_dir.empty()) platform_open_folder((pp / "assets/audio").string());
    }
    if (draw_tile(ui, window, sf::FloatRect({cx + (tw + gap) * 2.f, r2}, {tw, th}), UiIcon::Code, studio_tr(l, "Ren'Py .rpy", "Создать .rpy", ".rpy作成"), "scripts/story.rpy")) {
        studio_create_rpy_starter(st);
    }
    if (draw_tile(ui, window, sf::FloatRect({cx + (tw + gap) * 3.f, r2}, {tw, th}), UiIcon::Package, studio_tr(l, "Exports", "Релизы", "出力先"), "dist/")) {
        if (!st.active_project_dir.empty()) {
            std::error_code ec;
            std::filesystem::create_directories(pp / "dist", ec);
            platform_open_folder((pp / "dist").string());
        }
    }

    float log_y = r2 + th + 14.f;
    float log_h = std::max(110.f, (cy + ch) - log_y);
    render_console(st, ui, window, sf::FloatRect({cx, log_y}, {cw, log_h}), studio_tr(l, "Console", "Консоль", "コンソール"));
}

void studio_render_projects(LauncherState& st, UiContext& ui, sf::RenderWindow& window, float cx, float cy, float cw, float ch) {
    int l = st.lang_idx;
    ui_draw_rounded_panel(window, sf::FloatRect({cx, cy}, {cw, 96.f}), 3.f, COL_SURFACE, COL_BORDER, 1.f, 0);
    ui_draw_label(ui, window, studio_tr(l, "New Game in proj/", "Новая игра в proj/", "proj/ に新規作成"), {cx + 14.f, cy + 10.f}, 12, COL_MUTED);

    ui_text_input(ui, window, 101, sf::FloatRect({cx + 14.f, cy + 30.f}, {cw - 170.f, 32.f}), st.new_project_name, studio_tr(l, "Game name...", "Название игры...", "ゲーム名..."), COL_FG);

    if (ui_button(ui, window, sf::FloatRect({cx + cw - 146.f, cy + 30.f}, {132.f, 32.f}), studio_tr(l, "Create", "Создать", "作成"), COL_PRIMARY, COL_PRIMARY_HOV, COL_PRIMARY, 13, 3.f, UiIcon::Plus)) {
        studio_create_project_now(st, st.new_project_name);
        st.new_project_name.clear();
    }

    if (ui_checkbox(ui, window, {cx + 14.f, cy + 70.f}, 16.f, studio_tr(l, "Include test mod (neon_violet)", "Добавить тестовый мод (neon_violet)", "テストMODを含める (neon_violet)"), st.new_project_test_mod)) {
        st.new_project_test_mod = !st.new_project_test_mod;
    }

    float list_y = cy + 108.f;
    float list_h = std::max(120.f, (cy + ch) - list_y);
    ui_draw_rounded_panel(window, sf::FloatRect({cx, list_y}, {cw, list_h}), 3.f, COL_SURFACE, COL_BORDER, 1.f, 0);

    if (st.projects.empty()) {
        ui_draw_label_centered(ui, window, studio_tr(l, "No projects inside proj/", "В папке proj/ пока нет игр", "proj/ にプロジェクトがありません"), sf::FloatRect({cx, list_y + list_h * 0.45f}, {cw, 24.f}), 14, COL_DIM);
        return;
    }

    int max_rows = std::max(1, static_cast<int>((list_h - 18.f) / 60.f));
    for (size_t i = 0; i < st.projects.size() && i < static_cast<size_t>(max_rows); ++i) {
        const auto& p = st.projects[i];
        float ry = list_y + 10.f + static_cast<float>(i) * 60.f;
        bool is_active = (st.active_project_dir == p.dir);
        ui_draw_rounded_panel(window, sf::FloatRect({cx + 10.f, ry}, {cw - 20.f, 52.f}), 3.f, COL_BG, is_active ? COL_FG : COL_BORDER, 1.f, 0);

        float text_w = std::max(60.f, cw - 330.f);
        ui_draw_label_clamped(ui, window, p.name, {cx + 22.f, ry + 9.f}, 14, COL_FG, text_w);
        ui_draw_label_clamped(ui, window, p.dir, {cx + 22.f, ry + 29.f}, 11, COL_DIM, text_w);

        float bx = cx + cw - 18.f;
        bool confirming = (st.confirm_delete_idx == static_cast<int>(i));
        bx -= 74.f;
        if (ui_button(ui, window, sf::FloatRect({bx, ry + 11.f}, {74.f, 30.f}), confirming ? studio_tr(l, "Sure?", "Точно?", "確認?") : studio_tr(l, "Delete", "Удалить", "削除"), confirming ? sf::Color(127, 29, 29) : COL_SURFACE, confirming ? sf::Color(153, 27, 27) : sf::Color(48, 20, 24), confirming ? COL_ERR : COL_BORDER, 12, 2.f)) {
            if (confirming) {
                std::error_code ec;
                std::filesystem::remove_all(p.dir, ec);
                studio_log(st, "Deleted project: " + std::string(p.name));
                studio_scan_projects(st);
                return;
            } else {
                st.confirm_delete_idx = static_cast<int>(i);
            }
        }
        bx -= 78.f;
        if (ui_button(ui, window, sf::FloatRect({bx, ry + 11.f}, {70.f, 30.f}), studio_tr(l, "Clone", "Копия", "複製"), COL_SURFACE, COL_SURFACE_HOV, COL_BORDER, 12, 2.f)) {
            studio_duplicate_project(st, p);
            return;
        }
        bx -= 38.f;
        if (ui_button(ui, window, sf::FloatRect({bx, ry + 11.f}, {30.f, 30.f}), "", COL_SURFACE, COL_SURFACE_HOV, COL_BORDER, 12, 2.f, UiIcon::Folder)) {
            platform_open_folder(p.dir);
        }
        bx -= 86.f;
        if (is_active) {
            ui_draw_soft_badge(ui, window, sf::FloatRect({bx, ry + 14.f}, {78.f, 24.f}), studio_tr(l, "ACTIVE", "АКТИВЕН", "選択中"), COL_OK, 11);
        } else {
            if (ui_button(ui, window, sf::FloatRect({bx, ry + 11.f}, {78.f, 30.f}), studio_tr(l, "Select", "Выбрать", "選択"), COL_SURFACE, COL_SURFACE_HOV, COL_BORDER_HI, 12, 2.f)) {
                studio_select_project(st, p.name, p.dir);
                studio_save_launcher_config(st);
            }
        }
    }
}

void studio_render_assets(LauncherState& st, UiContext& ui, sf::RenderWindow& window, float cx, float cy, float cw, float ch) {
    int l = st.lang_idx;
    const char* cat_labels[7] = {
        studio_tr(l, "BG", "Фоны", "背景"),
        studio_tr(l, "Chars", "Персы", "立ち絵"),
        "UI",
        studio_tr(l, "Music", "Музыка", "BGM"),
        "SFX",
        studio_tr(l, "Voice", "Голос", "ボイス"),
        studio_tr(l, "Fonts", "Шрифты", "フォント")
    };

    float fx = cx;
    for (int i = 0; i < 7; ++i) {
        float bw = std::max(52.f, ui_measure_text_width(ui.font, cat_labels[i], 12) + 18.f);
        bool cur = (st.asset_filter_idx == i);
        if (ui_button(ui, window, sf::FloatRect({fx, cy}, {bw, 28.f}), cat_labels[i], cur ? COL_FG : COL_SURFACE, cur ? COL_FG : COL_SURFACE_HOV, cur ? COL_FG : COL_BORDER, 12, 2.f)) {
            st.asset_filter_idx = i;
            st.asset_subpath.clear();
            studio_scan_assets(st);
        }
        fx += bw + 6.f;
    }

    if (ui_button(ui, window, sf::FloatRect({cx + cw - 194.f, cy}, {92.f, 28.f}), studio_tr(l, "Refresh", "Обновить", "更新"), COL_SURFACE, COL_SURFACE_HOV, COL_BORDER, 12, 2.f)) {
        studio_scan_assets(st);
    }
    if (ui_button(ui, window, sf::FloatRect({cx + cw - 96.f, cy}, {96.f, 28.f}), studio_tr(l, "Open Dir", "Папка", "開く"), COL_SURFACE, COL_SURFACE_HOV, COL_BORDER, 12, 2.f, UiIcon::Folder)) {
        if (!st.active_project_dir.empty()) {
            std::filesystem::path d = std::filesystem::path(st.active_project_dir) / studio_asset_category_root(st.asset_filter_idx);
            if (!st.asset_subpath.empty()) d /= st.asset_subpath;
            std::error_code ec;
            std::filesystem::create_directories(d, ec);
            platform_open_folder(d.string());
        }
    }

    float bar_y = cy + 36.f;
    ui_draw_rounded_panel(window, sf::FloatRect({cx, bar_y}, {cw, 36.f}), 3.f, COL_SURFACE, COL_BORDER, 1.f, 0);

    float path_x = cx + 10.f;
    if (!st.asset_subpath.empty()) {
        if (ui_button(ui, window, sf::FloatRect({path_x, bar_y + 5.f}, {44.f, 26.f}), "..", COL_BG, COL_SURFACE_HOV, COL_BORDER, 12, 2.f)) {
            std::filesystem::path sp(st.asset_subpath);
            st.asset_subpath = sp.has_parent_path() ? sp.parent_path().generic_string() : "";
            studio_scan_assets(st);
            return;
        }
        path_x += 52.f;
    }

    std::string cur_dir_label = studio_asset_category_root(st.asset_filter_idx) + (st.asset_subpath.empty() ? "/" : ("/" + st.asset_subpath + "/"));
    float folder_ctrl_w = 276.f;
    ui_draw_label_clamped(ui, window, cur_dir_label, {path_x, bar_y + 10.f}, 12, COL_MUTED, std::max(40.f, cw - (path_x - cx) - folder_ctrl_w - 14.f));

    ui_text_input(ui, window, 102, sf::FloatRect({cx + cw - folder_ctrl_w, bar_y + 4.f}, {182.f, 28.f}), st.new_folder_name, studio_tr(l, "New folder (alice)...", "Новая папка...", "新規フォルダ..."), COL_FG);
    if (ui_button(ui, window, sf::FloatRect({cx + cw - 88.f, bar_y + 4.f}, {82.f, 28.f}), studio_tr(l, "+ Dir", "+ Папка", "+ 作成"), COL_BG, COL_SURFACE_HOV, COL_BORDER, 11, 2.f)) {
        studio_create_asset_subfolder(st, st.new_folder_name);
        st.new_folder_name.clear();
        return;
    }

    float list_y = bar_y + 44.f;
    float list_h = std::max(120.f, (cy + ch) - list_y);
    ui_draw_rounded_panel(window, sf::FloatRect({cx, list_y}, {cw, list_h}), 3.f, COL_SURFACE, COL_BORDER, 1.f, 0);

    int visible = std::max(1, static_cast<int>((list_h - 16.f) / 38.f));
    int max_scroll = std::max(0, static_cast<int>(st.assets.size()) - visible);
    if (ui.wheel_delta != 0.f) {
        st.asset_scroll = std::clamp(st.asset_scroll - static_cast<int>(ui.wheel_delta), 0, max_scroll);
    }

    if (st.assets.empty()) {
        std::string empty_msg = std::string(studio_tr(l, "Empty folder: ", "Пустая папка: ", "空のフォルダ: ")) + cur_dir_label;
        ui_draw_label_centered(ui, window, empty_msg, sf::FloatRect({cx, list_y + list_h * 0.45f}, {cw, 24.f}), 13, COL_DIM);
        return;
    }

    for (int r = 0; r < visible; ++r) {
        size_t idx = static_cast<size_t>(st.asset_scroll + r);
        if (idx >= st.assets.size()) break;
        const StudioAssetItem& item = st.assets[idx];
        float ry = list_y + 8.f + static_cast<float>(r) * 38.f;
        float row_w = max_scroll > 0 ? (cw - 28.f) : (cw - 16.f);
        sf::FloatRect row({cx + 8.f, ry}, {row_w, 32.f});
        bool hov = row.contains(ui.mouse_pos);

        ui_draw_rounded_panel(window, row, 2.f, hov ? COL_SURFACE_HOV : COL_BG, hov ? COL_BORDER_HI : COL_BORDER, 1.f, 0);
        ui_draw_soft_badge(ui, window, sf::FloatRect({cx + 16.f, ry + 6.f}, {52.f, 20.f}), item.category, item.is_dir ? COL_WARN : COL_MUTED, 10);

        std::string display_name = item.is_dir ? (item.name + "/") : item.name;
        ui_draw_label_clamped(ui, window, display_name, {cx + 78.f, ry + 8.f}, 12, COL_FG, std::max(60.f, row_w - 186.f));

        std::string meta = item.is_dir
            ? (std::to_string(item.size_bytes) + " " + studio_tr(l, "files", "файл.", "件"))
            : format_bytes(item.size_bytes);
        ui_draw_label(ui, window, meta, {cx + 8.f + row_w - 88.f, ry + 8.f}, 11, COL_DIM);

        if (hov && ui.mouse_clicked) {
            ui_consume_click(ui);
            if (item.is_dir) {
                st.asset_subpath = st.asset_subpath.empty() ? item.name : (st.asset_subpath + "/" + item.name);
                studio_scan_assets(st);
                return;
            } else {
                platform_open_in_editor(item.full_path, "");
            }
        }
    }

    if (max_scroll > 0) {
        ui_draw_scrollbar(window, sf::FloatRect({cx + cw - 12.f, list_y + 8.f}, {4.f, list_h - 16.f}), static_cast<int>(st.assets.size()), visible, st.asset_scroll, COL_MUTED);
    }
}

void studio_render_mods(LauncherState& st, UiContext& ui, sf::RenderWindow& window, float cx, float cy, float cw, float ch) {
    int l = st.lang_idx;
    float list_h = std::max(120.f, ch - 46.f);
    int visible_rows = std::max(1, static_cast<int>((list_h - 18.f) / 66.f));

    if (ui.wheel_delta != 0.f) {
        st.mod_scroll -= static_cast<int>(ui.wheel_delta);
        int max_scroll = std::max(0, static_cast<int>(st.mods.size()) - visible_rows);
        st.mod_scroll = std::clamp(st.mod_scroll, 0, max_scroll);
    }

    ui_draw_rounded_panel(window, sf::FloatRect({cx, cy}, {cw, list_h}), 3.f, COL_SURFACE, COL_BORDER, 1.f, 0);

    if (st.mods.empty()) {
        ui_draw_label_centered(ui, window, studio_tr(l, "No mods in active project", "В текущем проекте нет модов", "MODがありません"), sf::FloatRect({cx, cy + list_h * 0.45f}, {cw, 24.f}), 14, COL_DIM);
    } else {
        for (int r = 0; r < visible_rows; ++r) {
            size_t idx = static_cast<size_t>(st.mod_scroll + r);
            if (idx >= st.mods.size()) break;
            float y = cy + 10.f + static_cast<float>(r) * 66.f;
            QvnModInfo& m = st.mods[idx];

            ui_draw_rounded_panel(window, sf::FloatRect({cx + 10.f, y}, {cw - 20.f, 58.f}), 3.f, COL_BG, m.enabled ? COL_FG : COL_BORDER, 1.f, 0);

            if (ui_checkbox(ui, window, {cx + 22.f, y + 20.f}, 18.f, "", m.enabled != 0)) {
                m.enabled = m.enabled ? 0 : 1;
            }

            sf::Color bcol = m.is_engine_mod ? sf::Color(168, 85, 247) : COL_OK;
            ui_draw_soft_badge(ui, window, sf::FloatRect({cx + 52.f, y + 10.f}, {64.f, 20.f}), m.is_engine_mod ? "ENGINE" : "GAME", bcol, 10);

            float right_limit = cx + cw - 98.f;
            std::string title = std::string(m.name) + " v" + std::string(m.version) + " — " + std::string(m.author);
            ui_draw_label_clamped(ui, window, title, {cx + 126.f, y + 11.f}, 14, COL_FG, std::max(40.f, right_limit - (cx + 126.f)));
            ui_draw_label_clamped(ui, window, m.description[0] ? m.description : m.root_path, {cx + 52.f, y + 35.f}, 12, COL_DIM, std::max(40.f, right_limit - (cx + 52.f)));

            if (ui_button(ui, window, sf::FloatRect({cx + cw - 88.f, y + 14.f}, {28.f, 30.f}), "", COL_SURFACE, COL_SURFACE_HOV, COL_BORDER, 12, 2.f, UiIcon::ChevronUp)) {
                if (idx > 0) std::swap(st.mods[idx], st.mods[idx - 1]);
            }
            if (ui_button(ui, window, sf::FloatRect({cx + cw - 54.f, y + 14.f}, {28.f, 30.f}), "", COL_SURFACE, COL_SURFACE_HOV, COL_BORDER, 12, 2.f, UiIcon::ChevronDown)) {
                if (idx + 1 < st.mods.size()) std::swap(st.mods[idx], st.mods[idx + 1]);
            }
        }
    }

    float fy = cy + ch - 34.f;
    if (ui_button(ui, window, sf::FloatRect({cx, fy}, {108.f, 34.f}), studio_tr(l, "Rescan", "Обновить", "再読込"), COL_SURFACE, COL_SURFACE_HOV, COL_BORDER, 13, 3.f)) {
        studio_scan_mods(st);
    }
    if (ui_button(ui, window, sf::FloatRect({cx + 116.f, fy}, {142.f, 34.f}), studio_tr(l, "+ Engine Mod", "+ Мод движка", "+ エンジンMOD"), COL_SURFACE, COL_SURFACE_HOV, COL_BORDER, 12, 3.f, UiIcon::Plus)) {
        if (!st.active_project_dir.empty() && qvn_create_test_mod(st.active_project_dir.c_str())) {
            studio_scan_mods(st);
            studio_log(st, "Created engine mod neon_violet.");
        }
    }
    if (ui_button(ui, window, sf::FloatRect({cx + 266.f, fy}, {132.f, 34.f}), studio_tr(l, "+ Game Mod", "+ Мод игры", "+ ゲームMOD"), COL_SURFACE, COL_SURFACE_HOV, COL_BORDER, 12, 3.f, UiIcon::Plus)) {
        if (!st.active_project_dir.empty() && qvn_create_game_mod(st.active_project_dir.c_str(), "story_patch")) {
            studio_scan_mods(st);
            studio_log(st, "Created game mod story_patch.");
        }
    }
    if (ui_button(ui, window, sf::FloatRect({cx + 406.f, fy}, {108.f, 34.f}), studio_tr(l, "Save", "Сохранить", "保存"), COL_SURFACE, COL_SURFACE_HOV, COL_BORDER, 13, 3.f)) {
        studio_save_mods(st);
    }
    if (ui_button(ui, window, sf::FloatRect({cx + cw - 168.f, fy}, {168.f, 34.f}), studio_tr(l, "Save & Run", "Сохранить и запуск", "保存して実行"), COL_PRIMARY, COL_PRIMARY_HOV, COL_PRIMARY, 13, 3.f, UiIcon::Play)) {
        studio_save_mods(st);
        studio_launch_game(st);
    }
}

void studio_render_build(LauncherState& st, UiContext& ui, sf::RenderWindow& window, float cx, float cy, float cw, float ch) {
    int l = st.lang_idx;
    float left_w = 340.f;
    float right_x = cx + left_w + 14.f;
    float right_w = std::max(220.f, cw - left_w - 14.f);

    ui_draw_rounded_panel(window, sf::FloatRect({cx, cy}, {left_w, ch}), 3.f, COL_SURFACE, COL_BORDER, 1.f, 0);
    ui_draw_label(ui, window, studio_tr(l, "Target Platforms (5 OS)", "Платформы (5 ОС)", "ターゲット (5 OS)"), {cx + 16.f, cy + 12.f}, 13, COL_MUTED);
    ui_draw_divider(window, {cx, cy + 36.f}, left_w, COL_BORDER);

    const char* labels[5] = {"Linux (x86_64)", "Windows (.exe)", "macOS (Universal)", "Android (arm64)", "iOS (arm64)"};
    for (size_t i = 0; i < st.cfg.target_count && i < 5; ++i) {
        float y = cy + 48.f + static_cast<float>(i) * 34.f;
        if (ui_checkbox(ui, window, {cx + 16.f, y}, 18.f, labels[i], st.cfg.targets[i].enabled != 0)) {
            st.cfg.targets[i].enabled = st.cfg.targets[i].enabled ? 0 : 1;
        }
        bool has_stub = !studio_resolve_runtime_bin(st, st.cfg.targets[i].id, st.cfg.targets[i].game_bin).empty();
        ui_draw_soft_badge(ui, window, sf::FloatRect({cx + left_w - 82.f, y - 1.f}, {66.f, 20.f}), has_stub ? "STUB OK" : "NO STUB", has_stub ? COL_OK : COL_WARN, 10);
    }

    float opt_y = cy + 226.f;
    ui_draw_divider(window, {cx, opt_y}, left_w, COL_BORDER);
    ui_draw_label(ui, window, studio_tr(l, "Packaging & Encryption", "Упаковка и шифрование", "暗号化設定"), {cx + 16.f, opt_y + 12.f}, 13, COL_MUTED);

    if (ui_toggle(ui, window, {cx + 16.f, opt_y + 38.f}, studio_tr(l, "ChaCha20 Single File", "Шифровать в 1 файл", "単一ファイル暗号化"), st.pack_single_file, COL_FG)) {
        st.pack_single_file = !st.pack_single_file;
    }

    ui_draw_rounded_panel(window, sf::FloatRect({cx + 16.f, opt_y + 70.f}, {left_w - 108.f, 28.f}), 2.f, COL_BG, COL_BORDER, 1.f, 0);
    ui_draw_label_clamped(ui, window, st.cfg.pak_key, {cx + 24.f, opt_y + 77.f}, 11, COL_MUTED, left_w - 122.f);
    if (ui_button(ui, window, sf::FloatRect({cx + left_w - 86.f, opt_y + 70.f}, {70.f, 28.f}), studio_tr(l, "New Key", "Новый", "生成"), COL_BG, COL_SURFACE_HOV, COL_BORDER, 11, 2.f)) {
        studio_randomize_pak_key(st);
        studio_save_launcher_config(st);
    }

    bool busy = st.building.load();
    float btn_y = cy + ch - 92.f;
    if (ui_button(ui, window, sf::FloatRect({cx + 16.f, btn_y}, {left_w - 32.f, 36.f}), busy ? studio_tr(l, "Exporting...", "Экспорт...", "出力中...") : studio_tr(l, "Export Selected", "Экспорт выбранных", "選択出力"), busy ? COL_SURFACE_HOV : COL_PRIMARY, busy ? COL_SURFACE_HOV : COL_PRIMARY_HOV, busy ? COL_BORDER : COL_PRIMARY, 13, 3.f, UiIcon::Package)) {
        if (!busy) studio_start_multi_build(st);
    }
    if (ui_button(ui, window, sf::FloatRect({cx + 16.f, btn_y + 42.f}, {left_w - 32.f, 34.f}), studio_tr(l, "Quick Pack Current OS", "Быстрый пак текущей ОС", "現在OS用パック"), COL_BG, COL_SURFACE_HOV, COL_BORDER, 13, 3.f)) {
        if (!busy) studio_start_quick_pack(st);
    }

    render_console(st, ui, window, sf::FloatRect({right_x, cy}, {right_w, ch}), studio_tr(l, "Build Log", "Лог сборки", "ビルドログ"));
}

void studio_render_config_editor(LauncherState& st, UiContext& ui, sf::RenderWindow& window, float cx, float cy, float cw, float ch) {
    int l = st.lang_idx;
    ui_draw_rounded_panel(window, sf::FloatRect({cx, cy}, {cw, ch}), 3.f, COL_SURFACE, COL_BORDER, 1.f, 0);
    if (st.active_project_dir.empty()) {
        ui_draw_label_centered(ui, window, studio_tr(l, "Select a project first", "Сначала выберите проект", "プロジェクトを選択してください"), sf::FloatRect({cx, cy + ch * 0.45f}, {cw, 24.f}), 14, COL_DIM);
        return;
    }

    ui_draw_label(ui, window, studio_tr(l, "Game Configuration (config.json)", "Конфиг игры (config.json)", "ゲーム設定 (config.json)"), {cx + 18.f, cy + 14.f}, 16, COL_FG);
    ui_draw_divider(window, {cx, cy + 44.f}, cw, COL_BORDER);

    float col1_x = cx + 18.f;
    float col_w = (cw - 54.f) * 0.5f;
    float col2_x = col1_x + col_w + 18.f;
    float y = cy + 58.f;

    auto field = [&](int id, float fx, float fy, float fw, const char* lbl, std::string& buf) {
        ui_draw_label(ui, window, lbl, {fx, fy}, 11, COL_MUTED);
        std::string before = buf;
        ui_text_input(ui, window, id, sf::FloatRect({fx, fy + 16.f}, {fw, 30.f}), buf, "...", COL_FG);
        if (buf != before) st.game_cfg_dirty = true;
    };

    field(301, col1_x, y, col_w, studio_tr(l, "Window Title", "Заголовок окна", "ウィンドウ名"), st.gcfg_title);
    field(302, col2_x, y, col_w, studio_tr(l, "Author", "Автор", "作者"), st.gcfg_author);
    y += 56.f;
    field(303, col1_x, y, col_w, studio_tr(l, "Main Menu Title", "Заголовок меню", "タイトル画面名"), st.gcfg_menu_title);
    field(304, col2_x, y, col_w, studio_tr(l, "Entry Script", "Главный скрипт", "開始スクリプト"), st.gcfg_entry);
    y += 60.f;

    ui_draw_divider(window, {col1_x, y}, cw - 36.f, COL_BORDER);
    y += 14.f;
    ui_draw_label(ui, window, studio_tr(l, "Features & Menu Toggles", "Функции и пункты меню", "機能とメニュー設定"), {col1_x, y}, 12, COL_MUTED);
    y += 24.f;

    auto tog = [&](float tx, float ty, const char* lbl, uint8_t& val) {
        if (ui_toggle(ui, window, {tx, ty}, lbl, val != 0, COL_FG)) {
            val = val ? 0 : 1;
            st.game_cfg_dirty = true;
        }
    };

    tog(col1_x, y, studio_tr(l, "In-Game Mods", "Моды в игре", "ゲーム内MOD"), st.game_cfg.mods_enabled);
    tog(col2_x, y, studio_tr(l, "Single-File Saves (savedata.qvn)", "Сейвы одним файлом", "単一セーブファイル"), st.game_cfg.single_file_saves);
    y += 30.f;
    tog(col1_x, y, studio_tr(l, "Gallery", "Галерея", "ギャラリー"), st.game_cfg.gallery_enabled);
    tog(col2_x, y, studio_tr(l, "Achievements", "Достижения", "実績"), st.game_cfg.achievements_enabled);
    y += 30.f;
    tog(col1_x, y, studio_tr(l, "Dim Inactive Speakers", "Затемнять неактивных", "非話者を暗くする"), st.game_cfg.dim_inactive_chars);
    tog(col2_x, y, studio_tr(l, "Dev Inspector (F10)", "Инспектор (F10)", "開発インスペクタ"), st.game_cfg.dev_inspector_enabled);
    y += 30.f;
    tog(col1_x, y, studio_tr(l, "Character Breathing", "Дыхание спрайтов", "立ち絵呼吸"), st.game_cfg.idle_breathing);
    tog(col2_x, y, studio_tr(l, "Hot-Reload", "Хот-релоад", "ホットリロード"), st.game_cfg.hot_reload_enabled);
    y += 38.f;

    ui_draw_divider(window, {col1_x, y}, cw - 36.f, COL_BORDER);
    y += 14.f;

    auto srow = [&](int id, float sx, float sy, float sw, const char* lbl, float& v, float mn, float mx, const char* suf) {
        ui_draw_label(ui, window, lbl, {sx, sy}, 12, COL_MUTED);
        std::string vs = std::to_string(static_cast<int>(v)) + suf;
        ui_draw_label(ui, window, vs, {sx + sw - 46.f, sy}, 12, COL_FG);
        if (ui_slider(ui, window, id, sf::FloatRect({sx, sy + 20.f}, {sw, 8.f}), v, mn, mx, COL_FG)) {
            st.game_cfg_dirty = true;
        }
    };

    srow(311, col1_x, y, col_w, studio_tr(l, "Music Volume", "Музыка", "BGM音量"), st.game_cfg.music_volume, 0.f, 100.f, "%");
    srow(312, col2_x, y, col_w, studio_tr(l, "Voice Volume", "Голос", "ボイス音量"), st.game_cfg.voice_volume, 0.f, 100.f, "%");
    y += 46.f;
    srow(313, col1_x, y, col_w, studio_tr(l, "Text Speed", "Скорость текста", "文字速度"), st.game_cfg.typewriter_cps, 15.f, 120.f, " cps");
    float slots_f = static_cast<float>(st.game_cfg.save_slots_count);
    srow(314, col2_x, y, col_w, studio_tr(l, "Save Slots", "Слоты сейвов", "セーブ枠数"), slots_f, 6.f, 48.f, "");
    if (static_cast<uint32_t>(slots_f) != st.game_cfg.save_slots_count) {
        st.game_cfg.save_slots_count = static_cast<uint32_t>(slots_f);
        st.game_cfg_dirty = true;
    }

    float prev_y = y + 48.f;
    float prev_h = std::max(54.f, (cy + ch - 62.f) - prev_y);
    if (prev_h >= 54.f) {
        sf::Color box_c(st.game_cfg.box_color.r, st.game_cfg.box_color.g, st.game_cfg.box_color.b, 240);
        sf::Color box_br(st.game_cfg.box_outline_color.r, st.game_cfg.box_outline_color.g, st.game_cfg.box_outline_color.b, 180);
        sf::Color nm_c(st.game_cfg.name_color.r, st.game_cfg.name_color.g, st.game_cfg.name_color.b, 255);
        ui_draw_rounded_panel(window, sf::FloatRect({col1_x, prev_y}, {cw - 36.f, prev_h}), 3.f, box_c, box_br, 1.f, 0);
        ui_draw_label(ui, window, st.gcfg_menu_title.empty() ? "Alice" : st.gcfg_menu_title, {col1_x + 14.f, prev_y + 8.f}, 13, nm_c);
        ui_draw_label_clamped(ui, window, studio_tr(l, "Live dialogue box preview — configured directly without editing C++.", "Превью диалогового окна — всё настраивается через config.json.", "ダイアログボックスのライブプレビュー。"), {col1_x + 14.f, prev_y + 28.f}, 12, COL_FG, cw - 64.f);
    }

    float fy = cy + ch - 48.f;
    if (ui_button(ui, window, sf::FloatRect({col1_x, fy}, {140.f, 34.f}), studio_tr(l, "Reload JSON", "Перечитать", "再読込"), COL_BG, COL_SURFACE_HOV, COL_BORDER, 12, 3.f)) {
        studio_load_game_config(st);
    }
    if (ui_button(ui, window, sf::FloatRect({col1_x + 148.f, fy}, {154.f, 34.f}), studio_tr(l, "Open Raw JSON", "Открыть JSON", "JSONを開く"), COL_BG, COL_SURFACE_HOV, COL_BORDER, 12, 3.f, UiIcon::Code)) {
        platform_open_in_editor((std::filesystem::path(st.active_project_dir) / "config.json").string(), st.cfg.editor_command);
    }
    sf::Color sbg = st.game_cfg_dirty ? COL_PRIMARY : COL_BG;
    sf::Color shov = st.game_cfg_dirty ? COL_PRIMARY_HOV : COL_BG;
    sf::Color sbr = st.game_cfg_dirty ? COL_PRIMARY : COL_BORDER;
    if (ui_button(ui, window, sf::FloatRect({cx + cw - 178.f, fy}, {160.f, 34.f}), studio_tr(l, "Save config.json", "Сохранить конфиг", "設定を保存"), sbg, shov, sbr, 13, 3.f, UiIcon::Check)) {
        if (st.game_cfg_dirty) studio_save_game_config(st);
    }
}

void studio_render_settings(LauncherState& st, UiContext& ui, sf::RenderWindow& window, float cx, float cy, float cw, float ch) {
    int l = st.lang_idx;
    ui_draw_rounded_panel(window, sf::FloatRect({cx, cy}, {cw, ch}), 3.f, COL_SURFACE, COL_BORDER, 1.f, 0);
    ui_draw_label(ui, window, studio_tr(l, "Studio Settings", "Настройки Studio", "スタジオ設定"), {cx + 18.f, cy + 14.f}, 16, COL_FG);
    ui_draw_divider(window, {cx, cy + 44.f}, cw, COL_BORDER);

    float y = cy + 60.f;
    ui_draw_label(ui, window, studio_tr(l, "Studio FPS Limit", "Лимит FPS Studio", "Studio FPS制限"), {cx + 18.f, y + 6.f}, 13, COL_MUTED);
    const uint32_t fps_opts[4] = {30, 60, 120, 144};
    for (int i = 0; i < 4; ++i) {
        bool cur = (st.studio_fps_buf == fps_opts[i]);
        if (ui_button(ui, window, sf::FloatRect({cx + 220.f + static_cast<float>(i) * 74.f, y}, {66.f, 28.f}), std::to_string(fps_opts[i]), cur ? COL_FG : COL_BG, cur ? COL_FG : COL_SURFACE_HOV, cur ? COL_FG : COL_BORDER, 12, 2.f)) {
            st.studio_fps_buf = fps_opts[i];
            window.setFramerateLimit(fps_opts[i]);
        }
    }

    y += 44.f;
    ui_draw_label(ui, window, studio_tr(l, "New Project Template", "Шаблон новых игр", "新規テンプレート"), {cx + 18.f, y + 6.f}, 13, COL_MUTED);
    const char* tpl_names[3] = {
        studio_tr(l, "Standard JS", "Обычный JS", "標準 JS"),
        "Ren'Py .rpy",
        studio_tr(l, "Blank", "Пустой", "空")
    };
    for (int i = 0; i < 3; ++i) {
        bool cur = (st.default_template_buf == i);
        if (ui_button(ui, window, sf::FloatRect({cx + 220.f + static_cast<float>(i) * 116.f, y}, {108.f, 28.f}), tpl_names[i], cur ? COL_FG : COL_BG, cur ? COL_FG : COL_SURFACE_HOV, cur ? COL_FG : COL_BORDER, 12, 2.f)) {
            st.default_template_buf = static_cast<uint8_t>(i);
        }
    }

    y += 44.f;
    if (ui_toggle(ui, window, {cx + 18.f, y + 4.f}, studio_tr(l, "Open dist/ folder automatically after Pack / Export", "Автоматически открывать dist/ после упаковки", "パック後に dist/ フォルダを自動で開く"), st.auto_open_dist_buf, COL_FG)) {
        st.auto_open_dist_buf = !st.auto_open_dist_buf;
    }

    y += 44.f;
    ui_draw_label(ui, window, studio_tr(l, "Code Editor Command", "Команда редактора кода", "コードエディタ"), {cx + 18.f, y + 6.f}, 13, COL_MUTED);
    ui_text_input(ui, window, 201, sf::FloatRect({cx + 220.f, y}, {std::min(420.f, cw - 240.f), 32.f}), st.editor_cmd_buf, "code / zed / nvim / subl (empty = system default)", COL_FG);

    y += 40.f;
    const char* presets[5] = {"Auto", "code", "zed", "nvim", "subl"};
    for (int i = 0; i < 5; ++i) {
        if (ui_button(ui, window, sf::FloatRect({cx + 220.f + static_cast<float>(i) * 74.f, y}, {68.f, 24.f}), presets[i], COL_BG, COL_SURFACE_HOV, COL_BORDER, 11, 2.f)) {
            st.editor_cmd_buf = (i == 0) ? "" : presets[i];
        }
    }

    y += 42.f;
    ui_draw_label(ui, window, studio_tr(l, "Default PAK Key", "Ключ шифрования PAK", "既定PAKキー"), {cx + 18.f, y + 6.f}, 13, COL_MUTED);
    float key_w = std::min(340.f, cw - 330.f);
    ui_text_input(ui, window, 202, sf::FloatRect({cx + 220.f, y}, {key_w, 32.f}), st.pak_key_buf, "Secret ChaCha20 key...", COL_FG);
    if (ui_button(ui, window, sf::FloatRect({cx + 228.f + key_w, y}, {86.f, 32.f}), studio_tr(l, "Random", "Случайный", "生成"), COL_BG, COL_SURFACE_HOV, COL_BORDER, 12, 2.f)) {
        studio_randomize_pak_key(st);
    }

    y += 50.f;
    ui_draw_divider(window, {cx + 18.f, y}, cw - 36.f, COL_BORDER);
    y += 16.f;
    ui_draw_label(ui, window, studio_tr(l, "Maintenance & Paths", "Обслуживание и пути", "メンテナンスとパス"), {cx + 18.f, y}, 13, COL_MUTED);
    ui_draw_label_clamped(ui, window, "proj/:       " + st.proj_root, {cx + 18.f, y + 24.f}, 12, COL_DIM, cw - 220.f);
    ui_draw_label_clamped(ui, window, "runtimes/:   " + st.runtimes_dir, {cx + 18.f, y + 44.f}, 12, COL_DIM, cw - 220.f);
    if (ui_button(ui, window, sf::FloatRect({cx + cw - 188.f, y + 20.f}, {170.f, 32.f}), studio_tr(l, "Clear /tmp Cache", "Очистить кэш /tmp", "キャッシュ削除"), COL_BG, COL_SURFACE_HOV, COL_BORDER, 12, 2.f)) {
        studio_clear_tmp_cache(st);
    }

    float fy = cy + ch - 48.f;
    if (ui_button(ui, window, sf::FloatRect({cx + 18.f, fy}, {140.f, 34.f}), studio_tr(l, "Reset Defaults", "Сбросить", "初期化"), COL_BG, COL_SURFACE_HOV, COL_BORDER, 13, 3.f)) {
        qvn_launcher_config_default(&st.cfg);
        st.editor_cmd_buf = st.cfg.editor_command;
        st.pak_key_buf = st.cfg.pak_key;
        st.studio_fps_buf = st.cfg.studio_fps_limit;
        st.auto_open_dist_buf = st.cfg.auto_open_dist != 0;
        st.default_template_buf = st.cfg.default_template;
        window.setFramerateLimit(st.studio_fps_buf);
        studio_save_launcher_config(st);
    }
    if (ui_button(ui, window, sf::FloatRect({cx + 168.f, fy}, {152.f, 34.f}), studio_tr(l, "Open studio.json", "Открыть JSON", "JSONを開く"), COL_BG, COL_SURFACE_HOV, COL_BORDER, 13, 3.f)) {
        studio_save_launcher_config(st);
        platform_open_in_editor(st.launcher_cfg_path, st.cfg.editor_command);
    }

    bool settings_dirty = (st.editor_cmd_buf != std::string(st.cfg.editor_command))
        || (st.pak_key_buf != std::string(st.cfg.pak_key))
        || (st.studio_fps_buf != st.cfg.studio_fps_limit)
        || (st.auto_open_dist_buf != (st.cfg.auto_open_dist != 0))
        || (st.default_template_buf != st.cfg.default_template);

    sf::Color save_bg = settings_dirty ? COL_PRIMARY : COL_BG;
    sf::Color save_hov = settings_dirty ? COL_PRIMARY_HOV : COL_BG;
    sf::Color save_br = settings_dirty ? COL_PRIMARY : COL_BORDER;
    if (ui_button(ui, window, sf::FloatRect({cx + cw - 166.f, fy}, {148.f, 34.f}), studio_tr(l, "Save Settings", "Сохранить", "保存"), save_bg, save_hov, save_br, 13, 3.f, UiIcon::Check)) {
        if (settings_dirty) studio_save_launcher_config(st);
    }
}

void studio_render_about(LauncherState& st, UiContext& ui, sf::RenderWindow& window, float cx, float cy, float cw, float ch) {
    int l = st.lang_idx;
    ui_draw_rounded_panel(window, sf::FloatRect({cx, cy}, {cw, ch}), 3.f, COL_SURFACE, COL_BORDER, 1.f, 0);

    ui_draw_label(ui, window, "QuickVN Studio", {cx + 20.f, cy + 18.f}, 22, COL_FG);
    ui_draw_soft_badge(ui, window, sf::FloatRect({cx + 196.f, cy + 21.f}, {62.f, 22.f}), "v2.0.0", COL_OK, 11);
    ui_draw_label_clamped(ui, window, studio_tr(l, "Lightweight cross-platform visual novel engine and standalone studio.", "Лёгкий кроссплатформенный движок визуальных новелл и студия разработки.", "軽量クロスプラットフォーム・ビジュアルノベルエンジン＆スタジオ。"), {cx + 20.f, cy + 50.f}, 13, COL_MUTED, cw - 40.f);

    ui_draw_divider(window, {cx, cy + 80.f}, cw, COL_BORDER);

    float y = cy + 96.f;
    const char* specs[5][2] = {
        {"Core", "Rust staticlib (JSON, VFS, ChaCha20 PAK, Ren'Py transpiler, Saves, Mods)"},
        {"Scripting", "QuickJS (ES2023 JS DSL + .rpy transpiler)"},
        {"Renderer & Audio", "C++17 + SFML 3 (Graphics, Window, System, Audio)"},
        {"Mod System", "Built-in In-Game Mod Manager (engine_mods/ + mods/)"},
        {"Asset Layout", "assets/bg, assets/chars, assets/ui, assets/fonts, assets/audio/(music, sfx, voice)"}
    };
    for (int i = 0; i < 5; ++i) {
        ui_draw_label(ui, window, specs[i][0], {cx + 20.f, y}, 12, COL_DIM);
        ui_draw_label_clamped(ui, window, specs[i][1], {cx + 160.f, y}, 12, COL_FG, cw - 180.f);
        y += 26.f;
    }

    y += 10.f;
    ui_draw_divider(window, {cx + 20.f, y}, cw - 40.f, COL_BORDER);
    y += 16.f;
    ui_draw_label(ui, window, studio_tr(l, "Installed Runtime Stubs (5 OS)", "Установленные рантаймы (5 ОС)", "ランタイム状況 (5 OS)"), {cx + 20.f, y}, 13, COL_MUTED);
    y += 28.f;

    const char* plats[5] = {"linux", "windows", "macos", "android", "ios"};
    for (int i = 0; i < 5; ++i) {
        std::string bin = studio_resolve_runtime_bin(st, plats[i], st.cfg.targets[i].game_bin);
        bool ok = !bin.empty();
        ui_draw_label(ui, window, plats[i], {cx + 20.f, y + 2.f}, 12, COL_FG);
        ui_draw_soft_badge(ui, window, sf::FloatRect({cx + 110.f, y}, {66.f, 20.f}), ok ? "STUB OK" : "NO STUB", ok ? COL_OK : COL_WARN, 10);
        ui_draw_label_clamped(ui, window, ok ? bin : ("runtimes/" + std::string(plats[i]) + "/"), {cx + 190.f, y + 2.f}, 11, COL_DIM, cw - 210.f);
        y += 28.f;
    }

    float fy = cy + ch - 48.f;
    if (ui_button(ui, window, sf::FloatRect({cx + 20.f, fy}, {154.f, 34.f}), studio_tr(l, "Open Studio Dir", "Папка Studio", "Studioフォルダ"), COL_BG, COL_SURFACE_HOV, COL_BORDER, 12, 3.f, UiIcon::Folder)) {
        platform_open_folder(st.self_dir);
    }
    if (ui_button(ui, window, sf::FloatRect({cx + 184.f, fy}, {166.f, 34.f}), studio_tr(l, "Open Runtimes", "Папка runtimes/", "runtimesを開く"), COL_BG, COL_SURFACE_HOV, COL_BORDER, 12, 3.f, UiIcon::Package)) {
        platform_open_folder(st.runtimes_dir);
    }
    if (ui_button(ui, window, sf::FloatRect({cx + 360.f, fy}, {140.f, 34.f}), studio_tr(l, "Open proj/", "Папка proj/", "projを開く"), COL_BG, COL_SURFACE_HOV, COL_BORDER, 12, 3.f, UiIcon::Folder)) {
        platform_open_folder(st.proj_root);
    }
}