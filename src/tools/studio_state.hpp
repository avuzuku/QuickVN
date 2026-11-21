#pragma once
#include "rust_core.h"
#include "ui/ui.hpp"
#include <SFML/Graphics.hpp>
#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

enum class LauncherTab {
    Workspace = 0,
    Projects = 1,
    Assets = 2,
    ConfigEditor = 3,
    Mods = 4,
    Build = 5,
    Settings = 6,
    About = 7
};

struct StudioAssetItem {
    std::string category;
    std::string name;
    std::string rel_path;
    std::string full_path;
    uint64_t size_bytes = 0;
    bool is_dir = false;
};

struct LauncherState {
    QvnLauncherConfig cfg = {};
    std::string launcher_cfg_path;
    std::string self_dir = ".";
    std::string source_root;
    std::string proj_root = "proj";
    std::string runtimes_dir = "runtimes";
    std::string active_project_dir;
    std::string active_project_name;
    LauncherTab active_tab = LauncherTab::Workspace;
    int lang_idx = 0;

    std::vector<QvnProjectEntry> projects;
    std::string new_project_name;
    bool new_project_test_mod = true;
    int confirm_delete_idx = -1;

    std::vector<StudioAssetItem> assets;
    int asset_filter_idx = 0;
    std::string asset_subpath;
    std::string new_folder_name;
    int asset_scroll = 0;
    size_t total_asset_files = 0;

    std::vector<QvnModInfo> mods;
    int mod_scroll = 0;

    std::string editor_cmd_buf;
    std::string pak_key_buf;
    uint32_t studio_fps_buf = 60;
    bool auto_open_dist_buf = false;
    uint8_t default_template_buf = 0;

    QvnConfig game_cfg = {};
    bool game_cfg_loaded = false;
    bool game_cfg_dirty = false;
    std::string gcfg_title;
    std::string gcfg_author;
    std::string gcfg_entry;
    std::string gcfg_menu_title;
    std::string gcfg_menu_sub;

    bool pack_single_file = true;

    std::atomic<bool> building = false;
    std::mutex log_mutex;
    std::vector<std::string> logs;
    std::string status = "Ready";
    std::thread worker;
};

inline const sf::Color COL_BG(10, 10, 11);
inline const sf::Color COL_SIDEBAR(13, 13, 15);
inline const sf::Color COL_SURFACE(17, 17, 20);
inline const sf::Color COL_SURFACE_HOV(24, 24, 28);
inline const sf::Color COL_BORDER(36, 36, 42);
inline const sf::Color COL_BORDER_HI(68, 68, 78);
inline const sf::Color COL_FG(244, 244, 245);
inline const sf::Color COL_MUTED(138, 138, 147);
inline const sf::Color COL_DIM(82, 82, 91);
inline const sf::Color COL_PRIMARY(244, 244, 245);
inline const sf::Color COL_PRIMARY_HOV(218, 218, 224);
inline const sf::Color COL_OK(52, 211, 153);
inline const sf::Color COL_WARN(245, 158, 11);
inline const sf::Color COL_ERR(248, 113, 113);

const char* studio_tr(int lang, const char* en, const char* ru, const char* ja);
void studio_log(LauncherState& st, const std::string& msg);
void studio_set_status(LauncherState& st, const std::string& status);
std::string studio_host_platform_id();
std::string studio_find_source_root(const std::string& self_dir);
std::string studio_resolve_runtime_bin(const LauncherState& st, const std::string& platform_id, const std::string& configured_bin);
std::string studio_asset_category_root(int filter_idx);

void studio_scan_assets(LauncherState& st);
void studio_create_asset_subfolder(LauncherState& st, const std::string& raw_name);
void studio_scan_mods(LauncherState& st);
void studio_select_project(LauncherState& st, const std::string& name, const std::string& dir);
void studio_scan_projects(LauncherState& st);
void studio_create_project_now(LauncherState& st, const std::string& raw_name);
void studio_duplicate_project(LauncherState& st, const QvnProjectEntry& src);
void studio_validate_project(LauncherState& st);
void studio_create_rpy_starter(LauncherState& st);
void studio_randomize_pak_key(LauncherState& st);
void studio_save_launcher_config(LauncherState& st);
void studio_save_mods(LauncherState& st);
void studio_launch_game(LauncherState& st);
void studio_start_quick_pack(LauncherState& st);
void studio_start_multi_build(LauncherState& st);

void studio_render_workspace(LauncherState& st, UiContext& ui, sf::RenderWindow& window, float cx, float cy, float cw, float ch);
void studio_render_projects(LauncherState& st, UiContext& ui, sf::RenderWindow& window, float cx, float cy, float cw, float ch);
void studio_render_assets(LauncherState& st, UiContext& ui, sf::RenderWindow& window, float cx, float cy, float cw, float ch);
void studio_render_mods(LauncherState& st, UiContext& ui, sf::RenderWindow& window, float cx, float cy, float cw, float ch);
void studio_render_build(LauncherState& st, UiContext& ui, sf::RenderWindow& window, float cx, float cy, float cw, float ch);
void studio_render_settings(LauncherState& st, UiContext& ui, sf::RenderWindow& window, float cx, float cy, float cw, float ch);
void studio_load_game_config(LauncherState& st);
void studio_save_game_config(LauncherState& st);
void studio_clear_tmp_cache(LauncherState& st);
void studio_render_config_editor(LauncherState& st, UiContext& ui, sf::RenderWindow& window, float cx, float cy, float cw, float ch);
void studio_render_about(LauncherState& st, UiContext& ui, sf::RenderWindow& window, float cx, float cy, float cw, float ch);
