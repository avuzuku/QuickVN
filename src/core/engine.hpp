#pragma once
#include "audio/audio.hpp"
#include "core/command.hpp"
#include "core/mod_menu.hpp"
#include "graphics/scene.hpp"
#include "rust_core.h"
#include "script/qjs_bridge.h"
#include "ui/ui.hpp"
#include <SFML/Graphics.hpp>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

enum class GameScreen {
    MainMenu = 0,
    Playing,
    PauseMenu,
    SaveMenu,
    LoadMenu,
    SettingsMenu,
    BacklogMenu,
    GalleryMenu,
    AchievementsMenu,
    ModsMenu,
    PromptModal,
    EndScreen
};

struct BacklogEntry {
    std::string speaker;
    std::string text;
};

struct GalleryCardCache {
    QvnGalleryDef def = {};
    sf::Texture texture;
    bool tex_loaded = false;
};

struct EngineState {
    QvnConfig config = {};
    QvnPlayerSettings settings = {};
    std::string config_path = "config.json";
    std::string base_dir = ".";
    std::string raw_config_json;
    std::string active_locale_json;
    std::unordered_map<std::string, std::string> locale_cache;
    bool settings_dirty_save = false;

    sf::RenderWindow window;
    uint8_t created_window_mode = 255;
    sf::Clock frame_clock;
    UiContext ui;
    SceneState scene;
    AudioState audio;
    QjsBridge* script = nullptr;

    sf::Texture menu_bg_texture;
    std::optional<sf::Sprite> menu_bg_sprite;
    bool has_menu_bg = false;

    GameScreen screen = GameScreen::MainMenu;
    GameScreen return_screen = GameScreen::MainMenu;

    std::vector<QvnModInfo> active_mods;
    ModMenuState mod_menu;
    std::vector<Command> commands;
    size_t current_command_idx = 0;
    std::vector<int32_t> choice_history;
    std::vector<BacklogEntry> backlog;
    int backlog_scroll = 0;

    bool waiting_for_click = false;
    bool waiting_for_choice = false;
    int current_choice_callback_id = -1;
    int timed_choice_default_idx = 0;

    std::string prompt_text;
    std::string prompt_state_key;
    std::string prompt_buffer;

    float wait_timer = 0.f;
    bool auto_mode = false;
    float auto_timer = 0.f;
    bool skip_mode = false;
    float skip_timer = 0.f;
    bool ui_hidden = false;

    bool dev_inspector = false;
    float dev_fps = 60.f;

    std::string ending_title;
    std::string ending_subtitle;

    std::vector<QvnSaveSlotMeta> slot_metas;
    std::vector<std::optional<sf::Texture>> slot_thumbs;
    sf::Image last_scene_thumb;
    bool has_scene_thumb = false;
    std::vector<GalleryCardCache> gallery_items;
    std::vector<QvnAchievementDef> achievement_items;
    int gallery_lightbox_idx = -1;

    uint64_t script_fingerprint = 0;
    uint64_t config_fingerprint = 0;
    uint64_t mods_fingerprint = 0;
    float hot_reload_timer = 0.f;

    sf::Keyboard::Key key_advance = sf::Keyboard::Key::Space;
    sf::Keyboard::Key key_advance_alt = sf::Keyboard::Key::Enter;
    sf::Keyboard::Key key_quicksave = sf::Keyboard::Key::F5;
    sf::Keyboard::Key key_quickload = sf::Keyboard::Key::F9;
    sf::Keyboard::Key key_reload = sf::Keyboard::Key::F6;
    sf::Keyboard::Key key_exit = sf::Keyboard::Key::Escape;
    sf::Keyboard::Key key_backlog = sf::Keyboard::Key::L;
    sf::Keyboard::Key key_auto_mode = sf::Keyboard::Key::A;
    sf::Keyboard::Key key_skip_mode = sf::Keyboard::Key::Tab;
    sf::Keyboard::Key key_hide_ui = sf::Keyboard::Key::H;
};

bool engine_init(EngineState& eng, const std::string& config_path);
void engine_run(EngineState& eng);
void engine_shutdown(EngineState& eng);
