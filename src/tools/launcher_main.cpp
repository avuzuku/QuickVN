#include "tools/studio_state.hpp"
#include "platform/platform.hpp"
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <iostream>

int main(int argc, char** argv) {
    std::srand(static_cast<unsigned int>(std::time(nullptr)));
    LauncherState st;

    if (argc > 0 && argv[0] && argv[0][0] != '\0') {
        std::error_code ec;
        std::filesystem::path p = std::filesystem::weakly_canonical(argv[0], ec);
        if (!p.empty()) st.self_dir = p.parent_path().string();
    }

    st.source_root = studio_find_source_root(st.self_dir);
    std::filesystem::path sd(st.self_dir);
    st.launcher_cfg_path = (argc > 1 && argv[1] && argv[1][0] != '\0')
        ? std::string(argv[1])
        : (sd / "studio.json").string();

    qvn_launcher_config_default(&st.cfg);
    qvn_launcher_config_load(st.launcher_cfg_path.c_str(), &st.cfg);

    st.editor_cmd_buf = st.cfg.editor_command;
    st.pak_key_buf = st.cfg.pak_key;
    st.studio_fps_buf = st.cfg.studio_fps_limit >= 30 ? st.cfg.studio_fps_limit : 60;
    st.auto_open_dist_buf = st.cfg.auto_open_dist != 0;
    st.default_template_buf = st.cfg.default_template;
    std::string saved_lang(st.cfg.studio_lang);
    if (saved_lang == "ru") st.lang_idx = 1;
    else if (saved_lang == "ja") st.lang_idx = 2;
    else st.lang_idx = 0;

    std::filesystem::path proj_p(st.cfg.proj_root);
    if (proj_p.is_relative()) proj_p = sd / proj_p;
    st.proj_root = proj_p.string();

    std::filesystem::path rt_p(st.cfg.runtimes_dir);
    if (rt_p.is_relative()) rt_p = sd / rt_p;
    st.runtimes_dir = rt_p.string();

    std::error_code ec;
    std::filesystem::create_directories(st.proj_root, ec);
    std::filesystem::create_directories(st.runtimes_dir, ec);

    if (st.cfg.active_project[0] != '\0') st.active_project_name = st.cfg.active_project;

    studio_scan_projects(st);
    qvn_vfs_init(st.active_project_dir.empty() ? st.self_dir.c_str() : st.active_project_dir.c_str(), st.cfg.pak_key);
    studio_log(st, "QuickVN Studio ready.");

    sf::ContextSettings ctx;
    ctx.antiAliasingLevel = 4;
    sf::RenderWindow window(sf::VideoMode({1180, 720}), "QuickVN Studio", sf::Style::Default, sf::State::Windowed, ctx);
    window.setFramerateLimit(st.studio_fps_buf);

    UiContext ui;
    if (!ui_init(ui, "")) {
        std::cerr << "Launcher Error: Failed to load font." << std::endl;
        return -1;
    }

    sf::Clock clock;

    while (window.isOpen()) {
        float dt = clock.restart().asSeconds();
        ui_reset_frame(ui, dt);

        while (const std::optional ev = window.pollEvent()) {
            if (ev->is<sf::Event::Closed>()) {
                window.close();
            } else if (const auto* resized = ev->getIf<sf::Event::Resized>()) {
                window.setView(sf::View(sf::FloatRect({0.f, 0.f}, {static_cast<float>(resized->size.x), static_cast<float>(resized->size.y)})));
            } else {
                ui_handle_event(ui, *ev, window);
            }
        }

        float ww = static_cast<float>(window.getSize().x);
        float wh = static_cast<float>(window.getSize().y);
        int l = st.lang_idx;

        window.clear(COL_BG);

        float sb_w = 212.f;
        float top_h = 46.f;

        ui_draw_rounded_panel(window, sf::FloatRect({0.f, 0.f}, {sb_w, wh}), 0.f, COL_SIDEBAR, COL_BORDER, 1.f, 0);
        ui_draw_rounded_panel(window, sf::FloatRect({sb_w, 0.f}, {ww - sb_w, top_h}), 0.f, COL_SIDEBAR, COL_BORDER, 1.f, 0);

        ui_draw_label(ui, window, "QUICKVN", {18.f, 14.f}, 15, COL_FG);
        ui_draw_label(ui, window, "STUDIO", {94.f, 15.f}, 13, COL_DIM);

        std::string breadcrumb = st.active_project_name.empty() ? "proj/" : ("proj / " + st.active_project_name);
        ui_draw_label_clamped(ui, window, breadcrumb, {sb_w + 18.f, 15.f}, 13, COL_MUTED, std::max(60.f, ww - sb_w - 220.f));

        const char* lang_codes[3] = {"EN", "RU", "JA"};
        float lx = ww - 16.f;
        for (int i = 2; i >= 0; --i) {
            lx -= 36.f;
            bool cur = (st.lang_idx == i);
            if (ui_button(ui, window, sf::FloatRect({lx, 10.f}, {32.f, 26.f}), lang_codes[i], cur ? COL_FG : COL_BG, cur ? COL_FG : COL_SURFACE_HOV, cur ? COL_FG : COL_BORDER, 11, 2.f)) {
                if (st.lang_idx != i) {
                    st.lang_idx = i;
                    studio_save_launcher_config(st);
                }
            }
            lx -= 4.f;
        }

        if (ui_nav_item(ui, window, sf::FloatRect({10.f, 58.f}, {sb_w - 20.f, 34.f}), studio_tr(l, "Workspace", "Обзор", "ワークスペース"), UiIcon::Terminal, st.active_tab == LauncherTab::Workspace, COL_FG)) {
            st.active_tab = LauncherTab::Workspace;
        }
        if (ui_nav_item(ui, window, sf::FloatRect({10.f, 96.f}, {sb_w - 20.f, 34.f}), studio_tr(l, "Games", "Игры", "ゲーム"), UiIcon::Folder, st.active_tab == LauncherTab::Projects, COL_FG)) {
            studio_scan_projects(st);
            st.active_tab = LauncherTab::Projects;
        }
        if (ui_nav_item(ui, window, sf::FloatRect({10.f, 134.f}, {sb_w - 20.f, 34.f}), studio_tr(l, "Assets", "Ассеты", "素材"), UiIcon::Image, st.active_tab == LauncherTab::Assets, COL_FG)) {
            studio_scan_assets(st);
            st.active_tab = LauncherTab::Assets;
        }
        if (ui_nav_item(ui, window, sf::FloatRect({10.f, 172.f}, {sb_w - 20.f, 34.f}), studio_tr(l, "Game Config", "Конфиг игры", "ゲーム設定"), UiIcon::Sliders, st.active_tab == LauncherTab::ConfigEditor, COL_FG)) {
            studio_load_game_config(st);
            st.active_tab = LauncherTab::ConfigEditor;
        }
        if (ui_nav_item(ui, window, sf::FloatRect({10.f, 210.f}, {sb_w - 20.f, 34.f}), studio_tr(l, "Mods", "Моды", "MOD"), UiIcon::Layers, st.active_tab == LauncherTab::Mods, COL_FG)) {
            studio_scan_mods(st);
            st.active_tab = LauncherTab::Mods;
        }
        if (ui_nav_item(ui, window, sf::FloatRect({10.f, 248.f}, {sb_w - 20.f, 34.f}), studio_tr(l, "Export", "Экспорт", "出力"), UiIcon::Package, st.active_tab == LauncherTab::Build, COL_FG)) {
            st.active_tab = LauncherTab::Build;
        }

        ui_draw_divider(window, {14.f, 294.f}, sb_w - 28.f, COL_BORDER);

        if (ui_nav_item(ui, window, sf::FloatRect({10.f, 306.f}, {sb_w - 20.f, 34.f}), studio_tr(l, "Settings", "Настройки", "設定"), UiIcon::Sliders, st.active_tab == LauncherTab::Settings, COL_FG)) {
            st.active_tab = LauncherTab::Settings;
        }
        if (ui_nav_item(ui, window, sf::FloatRect({10.f, 344.f}, {sb_w - 20.f, 34.f}), studio_tr(l, "About", "О проекте", "情報"), UiIcon::Code, st.active_tab == LauncherTab::About, COL_FG)) {
            st.active_tab = LauncherTab::About;
        }

        std::string cur_status;
        {
            std::lock_guard<std::mutex> lock(st.log_mutex);
            cur_status = st.status;
        }

        size_t active_mods_cnt = 0;
        for (const auto& m : st.mods) if (m.enabled) active_mods_cnt++;

        float foot_y = wh - 98.f;
        ui_draw_divider(window, {0.f, foot_y - 10.f}, sb_w, COL_BORDER);
        sf::RectangleShape dot({6.f, 6.f});
        dot.setPosition({18.f, foot_y + 6.f});
        dot.setFillColor(st.building.load() ? COL_WARN : COL_OK);
        window.draw(dot);
        ui_draw_label_clamped(ui, window, cur_status, {32.f, foot_y + 1.f}, 13, COL_FG, sb_w - 44.f);
        ui_draw_label_clamped(ui, window, std::string(studio_tr(l, "Assets: ", "Ассеты: ", "素材: ")) + std::to_string(st.total_asset_files), {18.f, foot_y + 26.f}, 12, COL_DIM, sb_w - 32.f);
        ui_draw_label_clamped(ui, window, std::string(studio_tr(l, "Mods: ", "Моды: ", "MOD: ")) + std::to_string(active_mods_cnt) + "/" + std::to_string(st.mods.size()), {18.f, foot_y + 46.f}, 12, COL_DIM, sb_w - 32.f);
        ui_draw_label_clamped(ui, window, std::string(studio_tr(l, "Games: ", "Игры: ", "作品: ")) + std::to_string(st.projects.size()), {18.f, foot_y + 66.f}, 12, COL_DIM, sb_w - 32.f);

        float cx = sb_w + 16.f;
        float cy = top_h + 16.f;
        float cw = std::max(320.f, ww - sb_w - 32.f);
        float ch = std::max(240.f, wh - top_h - 32.f);

        if (st.active_tab == LauncherTab::Workspace) studio_render_workspace(st, ui, window, cx, cy, cw, ch);
        else if (st.active_tab == LauncherTab::Projects) studio_render_projects(st, ui, window, cx, cy, cw, ch);
        else if (st.active_tab == LauncherTab::Assets) studio_render_assets(st, ui, window, cx, cy, cw, ch);
        else if (st.active_tab == LauncherTab::ConfigEditor) studio_render_config_editor(st, ui, window, cx, cy, cw, ch);
        else if (st.active_tab == LauncherTab::Mods) studio_render_mods(st, ui, window, cx, cy, cw, ch);
        else if (st.active_tab == LauncherTab::Build) studio_render_build(st, ui, window, cx, cy, cw, ch);
        else if (st.active_tab == LauncherTab::Settings) studio_render_settings(st, ui, window, cx, cy, cw, ch);
        else studio_render_about(st, ui, window, cx, cy, cw, ch);

        window.display();
    }

    if (st.worker.joinable()) st.worker.join();
    qvn_vfs_shutdown();
    return 0;
}