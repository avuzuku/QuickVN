#pragma once
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct QvnColor {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
} QvnColor;

typedef struct QvnConfig {
    char window_title[128];
    uint32_t window_width;
    uint32_t window_height;
    uint32_t fps_limit;
    uint8_t fullscreen;
    uint8_t vsync;

    char font_path[256];
    char font_ru_path[256];
    char font_ja_path[256];
    char entry_script[256];
    char save_dir[256];
    char default_save_slot[64];

    QvnColor theme_accent_color;
    QvnColor theme_accent_hover_color;
    QvnColor theme_panel_color;
    QvnColor theme_panel_border_color;
    QvnColor theme_button_color;
    QvnColor theme_button_hover_color;
    float theme_corner_radius;
    uint8_t theme_shadow_alpha;

    uint8_t menu_enabled;
    char menu_title[128];
    char menu_subtitle[256];
    char menu_background[256];
    char menu_music[256];
    char menu_align[16];
    QvnColor menu_overlay_color;
    QvnColor menu_title_color;
    QvnColor menu_subtitle_color;

    uint8_t hud_enabled;
    char hud_position[16];
    uint8_t hud_show_backlog;
    uint8_t hud_show_auto;
    uint8_t hud_show_skip;
    uint8_t hud_show_save;
    uint8_t hud_show_load;
    uint8_t hud_show_hide;
    uint8_t hud_show_menu;

    float box_x;
    float box_y;
    float box_width;
    float box_height;
    float box_outline_thickness;
    QvnColor box_color;
    QvnColor box_outline_color;

    float name_x;
    float name_y;
    uint32_t name_font_size;
    QvnColor name_color;

    float text_x;
    float text_y;
    uint32_t text_font_size;
    float text_max_width;
    QvnColor text_color;

    float choice_width;
    float choice_height;
    float choice_spacing;
    float choice_center_y;
    uint32_t choice_font_size;
    float choice_outline_thickness;
    QvnColor choice_bg_color;
    QvnColor choice_hover_color;
    QvnColor choice_outline_color;
    QvnColor choice_hover_outline_color;
    QvnColor choice_text_color;

    uint8_t typewriter_enabled;
    float typewriter_cps;
    char typewriter_sound[256];
    uint32_t typewriter_sound_interval;
    float auto_delay;

    float bg_fade_duration;
    float char_fade_duration;
    float char_move_duration;
    float default_char_x;
    float default_char_y;
    uint8_t idle_breathing;
    float vignette_strength;

    float music_volume;
    float sound_volume;
    float voice_volume;

    uint8_t hot_reload_enabled;
    uint32_t hot_reload_interval_ms;
    uint8_t mods_enabled;

    char key_advance[32];
    char key_advance_alt[32];
    char key_quicksave[32];
    char key_quickload[32];
    char key_reload[32];
    char key_exit[32];
    char key_backlog[32];
    char key_auto_mode[32];
    char key_skip_mode[32];
    char key_hide_ui[32];

    char game_version[32];
    char game_author[128];
    uint8_t gallery_enabled;
    uint8_t achievements_enabled;
    uint8_t dev_inspector_enabled;
    uint8_t single_file_saves;
    uint32_t save_slots_count;
    uint32_t backlog_limit;
    uint32_t skip_interval_ms;

    uint8_t menu_show_continue;
    uint8_t menu_show_load;
    uint8_t menu_show_gallery;
    uint8_t menu_show_achievements;
    uint8_t menu_show_mods;
    uint8_t menu_show_settings;
    uint8_t menu_show_quit;
    char menu_logo[256];

    float dialogue_line_spacing;
    QvnColor nvl_bg_color;
    float nvl_x;
    float nvl_y;
    float nvl_max_width;
    uint32_t nvl_max_lines;

    uint8_t dim_inactive_chars;
    uint8_t inactive_char_brightness;

    char ui_sound_choice[256];
    char ui_sound_save[256];
    char ui_sound_load[256];
    char ui_sound_achievement[256];
} QvnConfig;

typedef struct QvnPlayerSettings {
    float music_volume;
    float sound_volume;
    float voice_volume;
    float typewriter_cps;
    float auto_delay;
    uint8_t typewriter_enabled;
    uint8_t idle_breathing;
    uint32_t res_width;
    uint32_t res_height;
    uint8_t window_mode;
    uint8_t scale_mode;
    char language[16];
} QvnPlayerSettings;

typedef struct QvnSaveData {
    uint64_t command_idx;
    int32_t* choice_history;
    size_t choice_count;
    char* js_state_json;
} QvnSaveData;

typedef struct QvnSaveSlotMeta {
    char slot[64];
    char speaker[64];
    char preview[128];
    char timestamp[32];
    uint64_t saved_epoch;
    uint8_t exists;
} QvnSaveSlotMeta;

typedef struct QvnGalleryDef {
    char id[64];
    char title[128];
    char image[256];
    uint8_t unlocked;
} QvnGalleryDef;

typedef struct QvnAchievementDef {
    char id[64];
    char title[128];
    char description[256];
    uint8_t unlocked;
} QvnAchievementDef;

typedef struct QvnProfileItems {
    QvnGalleryDef* gallery;
    size_t gallery_count;
    QvnAchievementDef* achievements;
    size_t achievement_count;
} QvnProfileItems;

typedef struct QvnModInfo {
    char id[128];
    char name[128];
    char version[32];
    char author[128];
    char description[256];
    char root_path[512];
    char hook_script[256];
    char config_patch[256];
    uint8_t is_engine_mod;
    uint8_t enabled;
} QvnModInfo;

typedef struct QvnModList {
    QvnModInfo* items;
    size_t count;
} QvnModList;

typedef struct QvnBytes {
    uint8_t* data;
    size_t len;
} QvnBytes;

typedef struct QvnBuildTarget {
    char id[32];
    char build_cmd[512];
    char game_bin[256];
    char out_exe[128];
    uint8_t enabled;
} QvnBuildTarget;

typedef struct QvnLauncherConfig {
    char proj_root[256];
    char active_project[128];
    char editor_command[256];
    char pak_key[128];
    char runtimes_dir[256];
    char studio_lang[8];
    uint32_t studio_fps_limit;
    uint8_t auto_open_dist;
    uint8_t default_template;
    QvnBuildTarget targets[5];
    size_t target_count;
} QvnLauncherConfig;

typedef struct QvnProjectEntry {
    char name[128];
    char dir[256];
} QvnProjectEntry;

typedef struct QvnProjectList {
    QvnProjectEntry* items;
    size_t count;
} QvnProjectList;

void qvn_config_default(QvnConfig* out_cfg);
int qvn_config_load_str(const char* json_str, QvnConfig* out_cfg);
int qvn_config_apply_json_str(const char* json_str, QvnConfig* inout_cfg);
const uint8_t* qvn_builtin_font_data(size_t* out_len);
int qvn_config_load_file(const char* path, QvnConfig* out_cfg);
int qvn_config_save_file(const char* path, const QvnConfig* cfg);
void qvn_saves_set_single_file(int enabled);

void qvn_settings_from_config(const QvnConfig* cfg, QvnPlayerSettings* out_settings);
int qvn_settings_load(const char* dir, QvnPlayerSettings* inout_settings);
int qvn_settings_save(const char* dir, const QvnPlayerSettings* settings);

int qvn_profile_load(const char* save_dir, const char* config_json_str, QvnProfileItems* out_items);
int qvn_profile_unlock_gallery(const char* save_dir, const char* id);
int qvn_profile_unlock_achievement(const char* save_dir, const char* id);
void qvn_profile_free(QvnProfileItems* items);

char* qvn_locale_lookup(const char* locale_json_str, const char* key);

uint64_t qvn_file_fingerprint(const char* path);

int qvn_save_state_ex(
    const char* dir,
    const char* slot,
    uint64_t command_idx,
    const int32_t* choices,
    size_t choice_count,
    const char* js_state_json,
    const char* speaker,
    const char* preview
);

int qvn_save_state_json(
    const char* dir,
    const char* slot,
    uint64_t command_idx,
    const int32_t* choices,
    size_t choice_count,
    const char* js_state_json
);

int qvn_load_state_json(
    const char* dir,
    const char* slot,
    QvnSaveData* out_data
);

int qvn_get_save_slot_meta(
    const char* dir,
    const char* slot,
    QvnSaveSlotMeta* out_meta
);

int qvn_find_latest_save_slot(
    const char* dir,
    char* out_slot_buf,
    size_t out_slot_cap
);

void qvn_free_save_data(QvnSaveData* data);

size_t qvn_utf8_char_count(const char* utf8_str);
size_t qvn_utf8_prefix_bytes(const char* utf8_str, size_t max_chars);

float qvn_lerp(float a, float b, float t);
float qvn_ease_out_cubic(float t);
float qvn_ease_in_out_quad(float t);

char* qvn_transpile_rpy(const char* rpy_source);
void qvn_free_cstr(char* str);

int qvn_mods_scan(const char* base_dir, QvnModList* out_list);
int qvn_mods_save(const char* base_dir, const QvnModInfo* items, size_t count);
void qvn_mods_free(QvnModList* list);
int qvn_create_test_mod(const char* project_dir);

int qvn_vfs_init(const char* base_dir, const char* pak_key);
void qvn_vfs_shutdown(void);
int qvn_vfs_has_pak(void);
int qvn_vfs_read_file(const char* virtual_path, QvnBytes* out_bytes);
int qvn_vfs_read_base_file(const char* virtual_path, QvnBytes* out_bytes);
void qvn_bytes_free(QvnBytes* bytes);

int qvn_pack_executable(
    const char* stub_exe,
    const char* output_exe,
    const char* project_dir,
    const char* pak_key
);

void qvn_launcher_config_default(QvnLauncherConfig* out_cfg);
int qvn_launcher_config_load(const char* path, QvnLauncherConfig* out_cfg);
int qvn_launcher_config_save(const char* path, const QvnLauncherConfig* cfg);

int qvn_projects_scan(const char* proj_root, QvnProjectList* out_list);
void qvn_projects_free(QvnProjectList* list);
int qvn_create_project(const char* font_src, const char* target_dir, const char* game_name, int with_test_mod, int template_id);
int qvn_create_game_mod(const char* project_dir, const char* mod_name);

#ifdef __cplusplus
}
#endif