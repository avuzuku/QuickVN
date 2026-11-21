use crate::saves::{is_single_file_saves, read_save_bundle, write_save_bundle};
use crate::json::{
    get_bool, get_color, get_f32, get_obj, get_str, get_u32, get_u8, json_parse, json_stringify,
    JsonValue,
};
use crate::types::{
    buf_to_string, copy_str_to_buf, QvnAchievementDef, QvnColor, QvnConfig, QvnGalleryDef,
    QvnPlayerSettings, QvnProfileItems,
};
use std::collections::{BTreeMap, BTreeSet};
use std::ffi::{CStr, CString};
use std::fs;
use std::os::raw::{c_char, c_int};
use std::path::{Path, PathBuf};
use std::ptr;

fn set_default_config(cfg: &mut QvnConfig) {
    cfg.window_title = [0; 128];
    copy_str_to_buf("QuickVN Novel", &mut cfg.window_title);
    cfg.window_width = 1280;
    cfg.window_height = 720;
    cfg.fps_limit = 60;
    cfg.fullscreen = 0;
    cfg.vsync = 1;

    cfg.font_path = [0; 256];
    copy_str_to_buf("assets/fonts/font.ttf", &mut cfg.font_path);
    cfg.font_ru_path = [0; 256];
    cfg.font_ja_path = [0; 256];
    cfg.entry_script = [0; 256];
    copy_str_to_buf("scripts/main.js", &mut cfg.entry_script);
    cfg.save_dir = [0; 256];
    copy_str_to_buf("saves", &mut cfg.save_dir);
    cfg.default_save_slot = [0; 64];
    copy_str_to_buf("quicksave", &mut cfg.default_save_slot);

    cfg.theme_accent_color = QvnColor { r: 99, g: 102, b: 241, a: 255 };
    cfg.theme_accent_hover_color = QvnColor { r: 129, g: 140, b: 248, a: 255 };
    cfg.theme_panel_color = QvnColor { r: 12, g: 13, b: 18, a: 240 };
    cfg.theme_panel_border_color = QvnColor { r: 255, g: 255, b: 255, a: 28 };
    cfg.theme_button_color = QvnColor { r: 24, g: 26, b: 36, a: 230 };
    cfg.theme_button_hover_color = QvnColor { r: 39, g: 42, b: 58, a: 248 };
    cfg.theme_corner_radius = 16.0;
    cfg.theme_shadow_alpha = 95;

    cfg.menu_enabled = 1;
    copy_str_to_buf("QuickVN", &mut cfg.menu_title);
    cfg.menu_subtitle = [0; 256];
    cfg.menu_background = [0; 256];
    cfg.menu_music = [0; 256];
    copy_str_to_buf("left", &mut cfg.menu_align);
    cfg.menu_overlay_color = QvnColor { r: 9, g: 10, b: 15, a: 232 };
    cfg.menu_title_color = QvnColor { r: 250, g: 250, b: 250, a: 255 };
    cfg.menu_subtitle_color = QvnColor { r: 161, g: 161, b: 170, a: 255 };

    cfg.hud_enabled = 1;
    copy_str_to_buf("bottom_right", &mut cfg.hud_position);
    cfg.hud_show_backlog = 1;
    cfg.hud_show_auto = 1;
    cfg.hud_show_skip = 1;
    cfg.hud_show_save = 1;
    cfg.hud_show_load = 1;
    cfg.hud_show_hide = 1;
    cfg.hud_show_menu = 1;

    cfg.box_x = 96.0;
    cfg.box_y = 496.0;
    cfg.box_width = 1088.0;
    cfg.box_height = 172.0;
    cfg.box_outline_thickness = 1.0;
    cfg.box_color = QvnColor { r: 10, g: 11, b: 16, a: 224 };
    cfg.box_outline_color = QvnColor { r: 255, g: 255, b: 255, a: 28 };

    cfg.name_x = 132.0;
    cfg.name_y = 512.0;
    cfg.name_font_size = 20;
    cfg.name_color = QvnColor { r: 250, g: 204, b: 21, a: 255 };

    cfg.text_x = 132.0;
    cfg.text_y = 548.0;
    cfg.text_font_size = 21;
    cfg.text_max_width = 1016.0;
    cfg.text_color = QvnColor { r: 244, g: 244, b: 245, a: 255 };

    cfg.choice_width = 580.0;
    cfg.choice_height = 54.0;
    cfg.choice_spacing = 12.0;
    cfg.choice_center_y = 245.0;
    cfg.choice_font_size = 19;
    cfg.choice_outline_thickness = 1.0;
    cfg.choice_bg_color = QvnColor { r: 14, g: 15, b: 22, a: 232 };
    cfg.choice_hover_color = QvnColor { r: 30, g: 33, b: 50, a: 248 };
    cfg.choice_outline_color = QvnColor { r: 255, g: 255, b: 255, a: 32 };
    cfg.choice_hover_outline_color = QvnColor { r: 129, g: 140, b: 248, a: 220 };
    cfg.choice_text_color = QvnColor { r: 244, g: 244, b: 245, a: 255 };

    cfg.typewriter_enabled = 1;
    cfg.typewriter_cps = 48.0;
    cfg.typewriter_sound = [0; 256];
    cfg.typewriter_sound_interval = 3;
    cfg.auto_delay = 2.0;

    cfg.bg_fade_duration = 0.45;
    cfg.char_fade_duration = 0.35;
    cfg.char_move_duration = 0.4;
    cfg.default_char_x = 640.0;
    cfg.default_char_y = 720.0;
    cfg.idle_breathing = 1;
    cfg.vignette_strength = 0.35;

    cfg.music_volume = 75.0;
    cfg.sound_volume = 100.0;
    cfg.voice_volume = 100.0;

    cfg.hot_reload_enabled = 1;
    cfg.hot_reload_interval_ms = 450;
    cfg.mods_enabled = 1;

    copy_str_to_buf("Space", &mut cfg.key_advance);
    copy_str_to_buf("Enter", &mut cfg.key_advance_alt);
    copy_str_to_buf("F5", &mut cfg.key_quicksave);
    copy_str_to_buf("F9", &mut cfg.key_quickload);
    copy_str_to_buf("F6", &mut cfg.key_reload);
    copy_str_to_buf("Escape", &mut cfg.key_exit);
    copy_str_to_buf("L", &mut cfg.key_backlog);
    copy_str_to_buf("A", &mut cfg.key_auto_mode);
    copy_str_to_buf("Tab", &mut cfg.key_skip_mode);
    copy_str_to_buf("H", &mut cfg.key_hide_ui);

    copy_str_to_buf("1.0.0", &mut cfg.game_version);
    copy_str_to_buf("Author", &mut cfg.game_author);
    cfg.gallery_enabled = 1;
    cfg.achievements_enabled = 1;
    cfg.dev_inspector_enabled = 1;
    cfg.single_file_saves = if cfg!(any(target_os = "android", target_os = "ios")) { 1 } else { 0 };
    cfg.save_slots_count = 24;
    cfg.backlog_limit = 200;
    cfg.skip_interval_ms = 70;

    cfg.menu_show_continue = 1;
    cfg.menu_show_load = 1;
    cfg.menu_show_gallery = 1;
    cfg.menu_show_achievements = 1;
    cfg.menu_show_mods = 1;
    cfg.menu_show_settings = 1;
    cfg.menu_show_quit = 1;
    cfg.menu_logo = [0; 256];

    cfg.dialogue_line_spacing = 1.38;
    cfg.nvl_bg_color = QvnColor { r: 8, g: 10, b: 16, a: 215 };
    cfg.nvl_x = 220.0;
    cfg.nvl_y = 96.0;
    cfg.nvl_max_width = 840.0;
    cfg.nvl_max_lines = 6;

    cfg.dim_inactive_chars = 1;
    cfg.inactive_char_brightness = 155;

    cfg.ui_sound_choice = [0; 256];
    cfg.ui_sound_save = [0; 256];
    cfg.ui_sound_load = [0; 256];
    cfg.ui_sound_achievement = [0; 256];
}

fn apply_json_to_config(root: &JsonValue, cfg: &mut QvnConfig) {
    if let Some(m) = get_obj(root, "window") {
        if let Some(v) = get_str(m, "title") { copy_str_to_buf(v, &mut cfg.window_title); }
        if let Some(v) = get_u32(m, "width") { cfg.window_width = v; }
        if let Some(v) = get_u32(m, "height") { cfg.window_height = v; }
        if let Some(v) = get_u32(m, "fps_limit") { cfg.fps_limit = v; }
        if let Some(v) = get_bool(m, "fullscreen") { cfg.fullscreen = v; }
        if let Some(v) = get_bool(m, "vsync") { cfg.vsync = v; }
    }
    if let Some(m) = get_obj(root, "paths") {
        if let Some(v) = get_str(m, "font") { copy_str_to_buf(v, &mut cfg.font_path); }
        if let Some(v) = get_str(m, "font_ru") { copy_str_to_buf(v, &mut cfg.font_ru_path); }
        if let Some(v) = get_str(m, "font_ja") { copy_str_to_buf(v, &mut cfg.font_ja_path); }
        if let Some(v) = get_str(m, "entry_script") { copy_str_to_buf(v, &mut cfg.entry_script); }
        if let Some(v) = get_str(m, "save_dir") { copy_str_to_buf(v, &mut cfg.save_dir); }
        if let Some(v) = get_str(m, "default_save_slot") { copy_str_to_buf(v, &mut cfg.default_save_slot); }
    }
    if let Some(m) = get_obj(root, "theme") {
        if let Some(v) = get_color(m, "accent_color") { cfg.theme_accent_color = v; }
        if let Some(v) = get_color(m, "accent_hover_color") { cfg.theme_accent_hover_color = v; }
        if let Some(v) = get_color(m, "panel_color") { cfg.theme_panel_color = v; }
        if let Some(v) = get_color(m, "panel_border_color") { cfg.theme_panel_border_color = v; }
        if let Some(v) = get_color(m, "button_color") { cfg.theme_button_color = v; }
        if let Some(v) = get_color(m, "button_hover_color") { cfg.theme_button_hover_color = v; }
        if let Some(v) = get_f32(m, "corner_radius") { cfg.theme_corner_radius = v.clamp(0.0, 40.0); }
        if let Some(v) = get_u8(m, "shadow_alpha") { cfg.theme_shadow_alpha = v; }
    }
    if let Some(m) = get_obj(root, "main_menu") {
        if let Some(v) = get_bool(m, "enabled") { cfg.menu_enabled = v; }
        if let Some(v) = get_str(m, "title") { copy_str_to_buf(v, &mut cfg.menu_title); }
        if let Some(v) = get_str(m, "subtitle") { copy_str_to_buf(v, &mut cfg.menu_subtitle); }
        if let Some(v) = get_str(m, "background") { copy_str_to_buf(v, &mut cfg.menu_background); }
        if let Some(v) = get_str(m, "music") { copy_str_to_buf(v, &mut cfg.menu_music); }
        if let Some(v) = get_str(m, "align") { copy_str_to_buf(v, &mut cfg.menu_align); }
        if let Some(v) = get_color(m, "overlay_color") { cfg.menu_overlay_color = v; }
        if let Some(v) = get_color(m, "title_color") { cfg.menu_title_color = v; }
        if let Some(v) = get_color(m, "subtitle_color") { cfg.menu_subtitle_color = v; }
    }
    if let Some(m) = get_obj(root, "hud") {
        if let Some(v) = get_bool(m, "enabled") { cfg.hud_enabled = v; }
        if let Some(v) = get_str(m, "position") { copy_str_to_buf(v, &mut cfg.hud_position); }
        if let Some(v) = get_bool(m, "show_backlog") { cfg.hud_show_backlog = v; }
        if let Some(v) = get_bool(m, "show_auto") { cfg.hud_show_auto = v; }
        if let Some(v) = get_bool(m, "show_skip") { cfg.hud_show_skip = v; }
        if let Some(v) = get_bool(m, "show_save") { cfg.hud_show_save = v; }
        if let Some(v) = get_bool(m, "show_load") { cfg.hud_show_load = v; }
        if let Some(v) = get_bool(m, "show_hide") { cfg.hud_show_hide = v; }
        if let Some(v) = get_bool(m, "show_menu") { cfg.hud_show_menu = v; }
    }
    if let Some(m) = get_obj(root, "dialogue_box") {
        if let Some(v) = get_f32(m, "x") { cfg.box_x = v; }
        if let Some(v) = get_f32(m, "y") { cfg.box_y = v; }
        if let Some(v) = get_f32(m, "width") { cfg.box_width = v; }
        if let Some(v) = get_f32(m, "height") { cfg.box_height = v; }
        if let Some(v) = get_f32(m, "outline_thickness") { cfg.box_outline_thickness = v; }
        if let Some(v) = get_color(m, "color") { cfg.box_color = v; }
        if let Some(v) = get_color(m, "outline_color") { cfg.box_outline_color = v; }
        if let Some(v) = get_f32(m, "name_x") { cfg.name_x = v; }
        if let Some(v) = get_f32(m, "name_y") { cfg.name_y = v; }
        if let Some(v) = get_u32(m, "name_font_size") { cfg.name_font_size = v; }
        if let Some(v) = get_color(m, "name_color") { cfg.name_color = v; }
        if let Some(v) = get_f32(m, "text_x") { cfg.text_x = v; }
        if let Some(v) = get_f32(m, "text_y") { cfg.text_y = v; }
        if let Some(v) = get_u32(m, "text_font_size") { cfg.text_font_size = v; }
        if let Some(v) = get_f32(m, "text_max_width") { cfg.text_max_width = v; }
        if let Some(v) = get_color(m, "text_color") { cfg.text_color = v; }
    }
    if let Some(m) = get_obj(root, "choices") {
        if let Some(v) = get_f32(m, "width") { cfg.choice_width = v; }
        if let Some(v) = get_f32(m, "height") { cfg.choice_height = v; }
        if let Some(v) = get_f32(m, "spacing") { cfg.choice_spacing = v; }
        if let Some(v) = get_f32(m, "center_y") { cfg.choice_center_y = v; }
        if let Some(v) = get_u32(m, "font_size") { cfg.choice_font_size = v; }
        if let Some(v) = get_f32(m, "outline_thickness") { cfg.choice_outline_thickness = v; }
        if let Some(v) = get_color(m, "bg_color") { cfg.choice_bg_color = v; }
        if let Some(v) = get_color(m, "hover_color") { cfg.choice_hover_color = v; }
        if let Some(v) = get_color(m, "outline_color") { cfg.choice_outline_color = v; }
        if let Some(v) = get_color(m, "hover_outline_color") { cfg.choice_hover_outline_color = v; }
        if let Some(v) = get_color(m, "text_color") { cfg.choice_text_color = v; }
    }
    if let Some(m) = get_obj(root, "typewriter") {
        if let Some(v) = get_bool(m, "enabled") { cfg.typewriter_enabled = v; }
        if let Some(v) = get_f32(m, "cps") { cfg.typewriter_cps = v; }
        if let Some(v) = get_str(m, "sound") { copy_str_to_buf(v, &mut cfg.typewriter_sound); }
        if let Some(v) = get_u32(m, "sound_interval") { cfg.typewriter_sound_interval = v.max(1); }
        if let Some(v) = get_f32(m, "auto_delay") { cfg.auto_delay = v.clamp(0.3, 10.0); }
    }
    if let Some(m) = get_obj(root, "transitions") {
        if let Some(v) = get_f32(m, "bg_fade_duration") { cfg.bg_fade_duration = v.max(0.0); }
        if let Some(v) = get_f32(m, "char_fade_duration") { cfg.char_fade_duration = v.max(0.0); }
        if let Some(v) = get_f32(m, "char_move_duration") { cfg.char_move_duration = v.max(0.0); }
        if let Some(v) = get_f32(m, "default_char_x") { cfg.default_char_x = v; }
        if let Some(v) = get_f32(m, "default_char_y") { cfg.default_char_y = v; }
        if let Some(v) = get_bool(m, "idle_breathing") { cfg.idle_breathing = v; }
        if let Some(v) = get_f32(m, "vignette_strength") { cfg.vignette_strength = v.clamp(0.0, 1.0); }
    }
    if let Some(m) = get_obj(root, "audio") {
        if let Some(v) = get_f32(m, "music_volume") { cfg.music_volume = v.clamp(0.0, 100.0); }
        if let Some(v) = get_f32(m, "sound_volume") { cfg.sound_volume = v.clamp(0.0, 100.0); }
        if let Some(v) = get_f32(m, "voice_volume") { cfg.voice_volume = v.clamp(0.0, 100.0); }
    }
    if let Some(m) = get_obj(root, "hot_reload") {
        if let Some(v) = get_bool(m, "enabled") { cfg.hot_reload_enabled = v; }
        if let Some(v) = get_u32(m, "interval_ms") { cfg.hot_reload_interval_ms = v.max(50); }
    }
    if let Some(m) = get_obj(root, "meta") {
        if let Some(v) = get_str(m, "version") { copy_str_to_buf(v, &mut cfg.game_version); }
        if let Some(v) = get_str(m, "author") { copy_str_to_buf(v, &mut cfg.game_author); }
    }
    if let Some(m) = get_obj(root, "features") {
        if let Some(v) = get_bool(m, "mods_enabled") { cfg.mods_enabled = v; }
        if let Some(v) = get_bool(m, "gallery_enabled") { cfg.gallery_enabled = v; }
        if let Some(v) = get_bool(m, "achievements_enabled") { cfg.achievements_enabled = v; }
        if let Some(v) = get_bool(m, "dev_inspector_enabled") { cfg.dev_inspector_enabled = v; }
        if let Some(v) = get_bool(m, "single_file_saves") { cfg.single_file_saves = v; }
        if let Some(v) = get_u32(m, "save_slots_count") { cfg.save_slots_count = v.clamp(6, 60); }
        if let Some(v) = get_u32(m, "backlog_limit") { cfg.backlog_limit = v.clamp(20, 1000); }
        if let Some(v) = get_u32(m, "skip_interval_ms") { cfg.skip_interval_ms = v.clamp(20, 500); }
    }
    if let Some(m) = get_obj(root, "main_menu") {
        if let Some(v) = get_bool(m, "show_continue") { cfg.menu_show_continue = v; }
        if let Some(v) = get_bool(m, "show_load") { cfg.menu_show_load = v; }
        if let Some(v) = get_bool(m, "show_gallery") { cfg.menu_show_gallery = v; }
        if let Some(v) = get_bool(m, "show_achievements") { cfg.menu_show_achievements = v; }
        if let Some(v) = get_bool(m, "show_mods") { cfg.menu_show_mods = v; }
        if let Some(v) = get_bool(m, "show_settings") { cfg.menu_show_settings = v; }
        if let Some(v) = get_bool(m, "show_quit") { cfg.menu_show_quit = v; }
        if let Some(v) = get_str(m, "logo") { copy_str_to_buf(v, &mut cfg.menu_logo); }
    }
    if let Some(m) = get_obj(root, "dialogue_box") {
        if let Some(v) = get_f32(m, "line_spacing") { cfg.dialogue_line_spacing = v.clamp(0.8, 2.5); }
    }
    if let Some(m) = get_obj(root, "nvl") {
        if let Some(v) = get_color(m, "bg_color") { cfg.nvl_bg_color = v; }
        if let Some(v) = get_f32(m, "x") { cfg.nvl_x = v; }
        if let Some(v) = get_f32(m, "y") { cfg.nvl_y = v; }
        if let Some(v) = get_f32(m, "max_width") { cfg.nvl_max_width = v.clamp(200.0, 1200.0); }
        if let Some(v) = get_u32(m, "max_lines") { cfg.nvl_max_lines = v.clamp(2, 16); }
    }
    if let Some(m) = get_obj(root, "transitions") {
        if let Some(v) = get_bool(m, "dim_inactive_chars") { cfg.dim_inactive_chars = v; }
        if let Some(v) = get_u8(m, "inactive_char_brightness") { cfg.inactive_char_brightness = v.clamp(60, 255); }
    }
    if let Some(m) = get_obj(root, "ui_sounds") {
        if let Some(v) = get_str(m, "choice") { copy_str_to_buf(v, &mut cfg.ui_sound_choice); }
        if let Some(v) = get_str(m, "save") { copy_str_to_buf(v, &mut cfg.ui_sound_save); }
        if let Some(v) = get_str(m, "load") { copy_str_to_buf(v, &mut cfg.ui_sound_load); }
        if let Some(v) = get_str(m, "achievement") { copy_str_to_buf(v, &mut cfg.ui_sound_achievement); }
    }
    if let JsonValue::Object(top) = root {
        if let Some(v) = get_bool(top, "mods_enabled") { cfg.mods_enabled = v; }
    }
    if let Some(m) = get_obj(root, "controls") {
        if let Some(v) = get_str(m, "advance") { copy_str_to_buf(v, &mut cfg.key_advance); }
        if let Some(v) = get_str(m, "advance_alt") { copy_str_to_buf(v, &mut cfg.key_advance_alt); }
        if let Some(v) = get_str(m, "quicksave") { copy_str_to_buf(v, &mut cfg.key_quicksave); }
        if let Some(v) = get_str(m, "quickload") { copy_str_to_buf(v, &mut cfg.key_quickload); }
        if let Some(v) = get_str(m, "reload") { copy_str_to_buf(v, &mut cfg.key_reload); }
        if let Some(v) = get_str(m, "exit") { copy_str_to_buf(v, &mut cfg.key_exit); }
        if let Some(v) = get_str(m, "backlog") { copy_str_to_buf(v, &mut cfg.key_backlog); }
        if let Some(v) = get_str(m, "auto_mode") { copy_str_to_buf(v, &mut cfg.key_auto_mode); }
        if let Some(v) = get_str(m, "skip_mode") { copy_str_to_buf(v, &mut cfg.key_skip_mode); }
        if let Some(v) = get_str(m, "hide_ui") { copy_str_to_buf(v, &mut cfg.key_hide_ui); }
    }
}

fn read_profile_sets(save_dir: &Path) -> (BTreeSet<String>, BTreeSet<String>) {
    let mut gal = BTreeSet::new();
    let mut ach = BTreeSet::new();
    let root_opt = if is_single_file_saves() {
        let mut bundle = read_save_bundle(save_dir);
        match bundle.remove("profile") {
            Some(JsonValue::Object(m)) => Some(m),
            _ => None,
        }
    } else {
        fs::read_to_string(save_dir.join("profile.json")).ok().and_then(|c| {
            match json_parse(&c) {
                Some(JsonValue::Object(m)) => Some(m),
                _ => None,
            }
        })
    };
    if let Some(root) = root_opt {
        if let Some(JsonValue::Array(arr)) = root.get("gallery") {
            for item in arr {
                if let JsonValue::Str(s) = item {
                    gal.insert(s.clone());
                }
            }
        }
        if let Some(JsonValue::Array(arr)) = root.get("achievements") {
            for item in arr {
                if let JsonValue::Str(s) = item {
                    ach.insert(s.clone());
                }
            }
        }
    }
    (gal, ach)
}

fn write_profile_sets(save_dir: &Path, gal: &BTreeSet<String>, ach: &BTreeSet<String>) -> bool {
    let gal_arr = gal.iter().map(|s| JsonValue::Str(s.clone())).collect();
    let ach_arr = ach.iter().map(|s| JsonValue::Str(s.clone())).collect();
    let mut root = BTreeMap::new();
    root.insert(String::from("gallery"), JsonValue::Array(gal_arr));
    root.insert(String::from("achievements"), JsonValue::Array(ach_arr));
    if is_single_file_saves() {
        let mut bundle = read_save_bundle(save_dir);
        bundle.insert(String::from("profile"), JsonValue::Object(root));
        return write_save_bundle(save_dir, &bundle);
    }
    if fs::create_dir_all(save_dir).is_err() {
        return false;
    }
    let mut out = String::new();
    json_stringify(&JsonValue::Object(root), &mut out);
    fs::write(save_dir.join("profile.json"), out.as_bytes()).is_ok()
}

fn color_to_json(c: QvnColor) -> JsonValue {
    JsonValue::Array(vec![
        JsonValue::Number(c.r as f64),
        JsonValue::Number(c.g as f64),
        JsonValue::Number(c.b as f64),
        JsonValue::Number(c.a as f64),
    ])
}

#[no_mangle]
pub extern "C" fn qvn_config_save_file(path: *const c_char, cfg_ptr: *const QvnConfig) -> c_int {
    if path.is_null() || cfg_ptr.is_null() {
        return 0;
    }
    let path_str = match unsafe { CStr::from_ptr(path) }.to_str() {
        Ok(s) if !s.is_empty() => s,
        _ => return 0,
    };
    let mut root = if let Ok(existing) = fs::read_to_string(path_str) {
        match json_parse(&existing) {
            Some(JsonValue::Object(m)) => m,
            _ => BTreeMap::new(),
        }
    } else {
        BTreeMap::new()
    };

    unsafe {
        let c = &*cfg_ptr;
        let mut meta = BTreeMap::new();
        meta.insert(String::from("version"), JsonValue::Str(buf_to_string(&c.game_version)));
        meta.insert(String::from("author"), JsonValue::Str(buf_to_string(&c.game_author)));
        root.insert(String::from("meta"), JsonValue::Object(meta));

        let mut win = BTreeMap::new();
        win.insert(String::from("title"), JsonValue::Str(buf_to_string(&c.window_title)));
        win.insert(String::from("width"), JsonValue::Number(c.window_width as f64));
        win.insert(String::from("height"), JsonValue::Number(c.window_height as f64));
        win.insert(String::from("fps_limit"), JsonValue::Number(c.fps_limit as f64));
        win.insert(String::from("fullscreen"), JsonValue::Bool(c.fullscreen != 0));
        win.insert(String::from("vsync"), JsonValue::Bool(c.vsync != 0));
        root.insert(String::from("window"), JsonValue::Object(win));

        let mut paths = BTreeMap::new();
        paths.insert(String::from("font"), JsonValue::Str(buf_to_string(&c.font_path)));
        paths.insert(String::from("font_ru"), JsonValue::Str(buf_to_string(&c.font_ru_path)));
        paths.insert(String::from("font_ja"), JsonValue::Str(buf_to_string(&c.font_ja_path)));
        paths.insert(String::from("entry_script"), JsonValue::Str(buf_to_string(&c.entry_script)));
        paths.insert(String::from("save_dir"), JsonValue::Str(buf_to_string(&c.save_dir)));
        paths.insert(String::from("default_save_slot"), JsonValue::Str(buf_to_string(&c.default_save_slot)));
        root.insert(String::from("paths"), JsonValue::Object(paths));

        let mut feats = BTreeMap::new();
        feats.insert(String::from("mods_enabled"), JsonValue::Bool(c.mods_enabled != 0));
        feats.insert(String::from("gallery_enabled"), JsonValue::Bool(c.gallery_enabled != 0));
        feats.insert(String::from("achievements_enabled"), JsonValue::Bool(c.achievements_enabled != 0));
        feats.insert(String::from("dev_inspector_enabled"), JsonValue::Bool(c.dev_inspector_enabled != 0));
        feats.insert(String::from("single_file_saves"), JsonValue::Bool(c.single_file_saves != 0));
        feats.insert(String::from("save_slots_count"), JsonValue::Number(c.save_slots_count as f64));
        feats.insert(String::from("backlog_limit"), JsonValue::Number(c.backlog_limit as f64));
        feats.insert(String::from("skip_interval_ms"), JsonValue::Number(c.skip_interval_ms as f64));
        root.insert(String::from("features"), JsonValue::Object(feats));

        let mut menu = BTreeMap::new();
        menu.insert(String::from("enabled"), JsonValue::Bool(c.menu_enabled != 0));
        menu.insert(String::from("title"), JsonValue::Str(buf_to_string(&c.menu_title)));
        menu.insert(String::from("subtitle"), JsonValue::Str(buf_to_string(&c.menu_subtitle)));
        menu.insert(String::from("background"), JsonValue::Str(buf_to_string(&c.menu_background)));
        menu.insert(String::from("music"), JsonValue::Str(buf_to_string(&c.menu_music)));
        menu.insert(String::from("align"), JsonValue::Str(buf_to_string(&c.menu_align)));
        menu.insert(String::from("logo"), JsonValue::Str(buf_to_string(&c.menu_logo)));
        menu.insert(String::from("show_continue"), JsonValue::Bool(c.menu_show_continue != 0));
        menu.insert(String::from("show_load"), JsonValue::Bool(c.menu_show_load != 0));
        menu.insert(String::from("show_gallery"), JsonValue::Bool(c.menu_show_gallery != 0));
        menu.insert(String::from("show_achievements"), JsonValue::Bool(c.menu_show_achievements != 0));
        menu.insert(String::from("show_mods"), JsonValue::Bool(c.menu_show_mods != 0));
        menu.insert(String::from("show_settings"), JsonValue::Bool(c.menu_show_settings != 0));
        menu.insert(String::from("show_quit"), JsonValue::Bool(c.menu_show_quit != 0));
        root.insert(String::from("main_menu"), JsonValue::Object(menu));

        let mut theme = BTreeMap::new();
        theme.insert(String::from("accent_color"), color_to_json(c.theme_accent_color));
        theme.insert(String::from("accent_hover_color"), color_to_json(c.theme_accent_hover_color));
        theme.insert(String::from("panel_color"), color_to_json(c.theme_panel_color));
        theme.insert(String::from("panel_border_color"), color_to_json(c.theme_panel_border_color));
        theme.insert(String::from("button_color"), color_to_json(c.theme_button_color));
        theme.insert(String::from("button_hover_color"), color_to_json(c.theme_button_hover_color));
        theme.insert(String::from("corner_radius"), JsonValue::Number(c.theme_corner_radius as f64));
        theme.insert(String::from("shadow_alpha"), JsonValue::Number(c.theme_shadow_alpha as f64));
        root.insert(String::from("theme"), JsonValue::Object(theme));

        let mut dlg = BTreeMap::new();
        dlg.insert(String::from("x"), JsonValue::Number(c.box_x as f64));
        dlg.insert(String::from("y"), JsonValue::Number(c.box_y as f64));
        dlg.insert(String::from("width"), JsonValue::Number(c.box_width as f64));
        dlg.insert(String::from("height"), JsonValue::Number(c.box_height as f64));
        dlg.insert(String::from("color"), color_to_json(c.box_color));
        dlg.insert(String::from("outline_color"), color_to_json(c.box_outline_color));
        dlg.insert(String::from("name_font_size"), JsonValue::Number(c.name_font_size as f64));
        dlg.insert(String::from("name_color"), color_to_json(c.name_color));
        dlg.insert(String::from("text_font_size"), JsonValue::Number(c.text_font_size as f64));
        dlg.insert(String::from("text_color"), color_to_json(c.text_color));
        dlg.insert(String::from("line_spacing"), JsonValue::Number(c.dialogue_line_spacing as f64));
        root.insert(String::from("dialogue_box"), JsonValue::Object(dlg));

        let mut nvl = BTreeMap::new();
        nvl.insert(String::from("bg_color"), color_to_json(c.nvl_bg_color));
        nvl.insert(String::from("x"), JsonValue::Number(c.nvl_x as f64));
        nvl.insert(String::from("y"), JsonValue::Number(c.nvl_y as f64));
        nvl.insert(String::from("max_width"), JsonValue::Number(c.nvl_max_width as f64));
        nvl.insert(String::from("max_lines"), JsonValue::Number(c.nvl_max_lines as f64));
        root.insert(String::from("nvl"), JsonValue::Object(nvl));

        let mut tw = BTreeMap::new();
        tw.insert(String::from("enabled"), JsonValue::Bool(c.typewriter_enabled != 0));
        tw.insert(String::from("cps"), JsonValue::Number(c.typewriter_cps as f64));
        tw.insert(String::from("sound"), JsonValue::Str(buf_to_string(&c.typewriter_sound)));
        tw.insert(String::from("sound_interval"), JsonValue::Number(c.typewriter_sound_interval as f64));
        tw.insert(String::from("auto_delay"), JsonValue::Number(c.auto_delay as f64));
        root.insert(String::from("typewriter"), JsonValue::Object(tw));

        let mut tr = BTreeMap::new();
        tr.insert(String::from("bg_fade_duration"), JsonValue::Number(c.bg_fade_duration as f64));
        tr.insert(String::from("char_fade_duration"), JsonValue::Number(c.char_fade_duration as f64));
        tr.insert(String::from("char_move_duration"), JsonValue::Number(c.char_move_duration as f64));
        tr.insert(String::from("idle_breathing"), JsonValue::Bool(c.idle_breathing != 0));
        tr.insert(String::from("vignette_strength"), JsonValue::Number(c.vignette_strength as f64));
        tr.insert(String::from("dim_inactive_chars"), JsonValue::Bool(c.dim_inactive_chars != 0));
        tr.insert(String::from("inactive_char_brightness"), JsonValue::Number(c.inactive_char_brightness as f64));
        root.insert(String::from("transitions"), JsonValue::Object(tr));

        let mut aud = BTreeMap::new();
        aud.insert(String::from("music_volume"), JsonValue::Number(c.music_volume as f64));
        aud.insert(String::from("sound_volume"), JsonValue::Number(c.sound_volume as f64));
        aud.insert(String::from("voice_volume"), JsonValue::Number(c.voice_volume as f64));
        root.insert(String::from("audio"), JsonValue::Object(aud));

        let mut hr = BTreeMap::new();
        hr.insert(String::from("enabled"), JsonValue::Bool(c.hot_reload_enabled != 0));
        hr.insert(String::from("interval_ms"), JsonValue::Number(c.hot_reload_interval_ms as f64));
        root.insert(String::from("hot_reload"), JsonValue::Object(hr));
    }

    let mut out = String::new();
    json_stringify(&JsonValue::Object(root), &mut out);
    if fs::write(path_str, out.as_bytes()).is_ok() { 1 } else { 0 }
}

#[no_mangle]
pub extern "C" fn qvn_config_default(out_cfg: *mut QvnConfig) {
    if out_cfg.is_null() {
        return;
    }
    unsafe {
        set_default_config(&mut *out_cfg);
    }
}

#[no_mangle]
pub extern "C" fn qvn_config_load_str(json_str: *const c_char, out_cfg: *mut QvnConfig) -> c_int {
    if out_cfg.is_null() {
        return 0;
    }
    unsafe {
        set_default_config(&mut *out_cfg);
    }
    qvn_config_apply_json_str(json_str, out_cfg)
}

#[no_mangle]
pub extern "C" fn qvn_config_apply_json_str(json_str: *const c_char, inout_cfg: *mut QvnConfig) -> c_int {
    if inout_cfg.is_null() || json_str.is_null() {
        return 0;
    }
    let text = match unsafe { CStr::from_ptr(json_str) }.to_str() {
        Ok(s) => s,
        Err(_) => return 0,
    };
    let parsed = match json_parse(text) {
        Some(v) => v,
        None => return 0,
    };
    unsafe {
        apply_json_to_config(&parsed, &mut *inout_cfg);
    }
    1
}

#[no_mangle]
pub extern "C" fn qvn_config_load_file(path: *const c_char, out_cfg: *mut QvnConfig) -> c_int {
    if out_cfg.is_null() {
        return 0;
    }
    unsafe {
        set_default_config(&mut *out_cfg);
    }
    if path.is_null() {
        return 0;
    }
    let path_str = match unsafe { CStr::from_ptr(path) }.to_str() {
        Ok(s) => s,
        Err(_) => return 0,
    };
    let content = match fs::read_to_string(path_str) {
        Ok(s) => s,
        Err(_) => return 0,
    };
    let parsed = match json_parse(&content) {
        Some(v) => v,
        None => return 0,
    };
    unsafe {
        apply_json_to_config(&parsed, &mut *out_cfg);
    }
    1
}

#[no_mangle]
pub extern "C" fn qvn_settings_from_config(cfg: *const QvnConfig, out_settings: *mut QvnPlayerSettings) {
    if cfg.is_null() || out_settings.is_null() {
        return;
    }
    unsafe {
        (*out_settings).music_volume = (*cfg).music_volume;
        (*out_settings).sound_volume = (*cfg).sound_volume;
        (*out_settings).voice_volume = (*cfg).voice_volume;
        (*out_settings).typewriter_cps = (*cfg).typewriter_cps;
        (*out_settings).auto_delay = (*cfg).auto_delay;
        (*out_settings).typewriter_enabled = (*cfg).typewriter_enabled;
        (*out_settings).idle_breathing = (*cfg).idle_breathing;
        (*out_settings).res_width = (*cfg).window_width;
        (*out_settings).res_height = (*cfg).window_height;
        (*out_settings).window_mode = if (*cfg).fullscreen != 0 { 2 } else { 0 };
        (*out_settings).scale_mode = 0;
        (*out_settings).language = [0; 16];
        copy_str_to_buf("en", &mut (*out_settings).language);
    }
}

#[no_mangle]
pub extern "C" fn qvn_settings_load(dir: *const c_char, inout_settings: *mut QvnPlayerSettings) -> c_int {
    if dir.is_null() || inout_settings.is_null() {
        return 0;
    }
    let dir_str = match unsafe { CStr::from_ptr(dir) }.to_str() {
        Ok(s) => s,
        Err(_) => return 0,
    };
    let dir_path = Path::new(dir_str);
    let m = if is_single_file_saves() {
        let mut bundle = read_save_bundle(dir_path);
        match bundle.remove("settings") {
            Some(JsonValue::Object(map)) => map,
            _ => return 0,
        }
    } else {
        let content = match fs::read_to_string(dir_path.join("settings.json")) {
            Ok(s) => s,
            Err(_) => return 0,
        };
        match json_parse(&content) {
            Some(JsonValue::Object(map)) => map,
            _ => return 0,
        }
    };
    unsafe {
        if let Some(v) = get_f32(&m, "music_volume") { (*inout_settings).music_volume = v.clamp(0.0, 100.0); }
        if let Some(v) = get_f32(&m, "sound_volume") { (*inout_settings).sound_volume = v.clamp(0.0, 100.0); }
        if let Some(v) = get_f32(&m, "voice_volume") { (*inout_settings).voice_volume = v.clamp(0.0, 100.0); }
        if let Some(v) = get_f32(&m, "typewriter_cps") { (*inout_settings).typewriter_cps = v.clamp(10.0, 150.0); }
        if let Some(v) = get_f32(&m, "auto_delay") { (*inout_settings).auto_delay = v.clamp(0.3, 10.0); }
        if let Some(v) = get_bool(&m, "typewriter_enabled") { (*inout_settings).typewriter_enabled = v; }
        if let Some(v) = get_bool(&m, "idle_breathing") { (*inout_settings).idle_breathing = v; }
        if let Some(v) = get_u32(&m, "res_width") { if v >= 640 { (*inout_settings).res_width = v; } }
        if let Some(v) = get_u32(&m, "res_height") { if v >= 360 { (*inout_settings).res_height = v; } }
        if let Some(v) = get_u8(&m, "window_mode") { (*inout_settings).window_mode = v.min(2); }
        if let Some(v) = get_u8(&m, "scale_mode") { (*inout_settings).scale_mode = v.min(1); }
        if let Some(v) = get_str(&m, "language") { copy_str_to_buf(v, &mut (*inout_settings).language); }
    }
    1
}

#[no_mangle]
pub extern "C" fn qvn_settings_save(dir: *const c_char, settings: *const QvnPlayerSettings) -> c_int {
    if dir.is_null() || settings.is_null() {
        return 0;
    }
    let dir_str = match unsafe { CStr::from_ptr(dir) }.to_str() {
        Ok(s) => s,
        Err(_) => return 0,
    };
    let dir_path = PathBuf::from(dir_str);
    if fs::create_dir_all(&dir_path).is_err() {
        return 0;
    }
    let mut root = BTreeMap::new();
    unsafe {
        root.insert(String::from("music_volume"), JsonValue::Number((*settings).music_volume as f64));
        root.insert(String::from("sound_volume"), JsonValue::Number((*settings).sound_volume as f64));
        root.insert(String::from("voice_volume"), JsonValue::Number((*settings).voice_volume as f64));
        root.insert(String::from("typewriter_cps"), JsonValue::Number((*settings).typewriter_cps as f64));
        root.insert(String::from("auto_delay"), JsonValue::Number((*settings).auto_delay as f64));
        root.insert(String::from("typewriter_enabled"), JsonValue::Bool((*settings).typewriter_enabled != 0));
        root.insert(String::from("idle_breathing"), JsonValue::Bool((*settings).idle_breathing != 0));
        root.insert(String::from("res_width"), JsonValue::Number((*settings).res_width as f64));
        root.insert(String::from("res_height"), JsonValue::Number((*settings).res_height as f64));
        root.insert(String::from("window_mode"), JsonValue::Number((*settings).window_mode as f64));
        root.insert(String::from("scale_mode"), JsonValue::Number((*settings).scale_mode as f64));
        root.insert(String::from("language"), JsonValue::Str(buf_to_string(&(*settings).language)));
    }
    if is_single_file_saves() {
        let mut bundle = read_save_bundle(&dir_path);
        bundle.insert(String::from("settings"), JsonValue::Object(root));
        return if write_save_bundle(&dir_path, &bundle) { 1 } else { 0 };
    }
    let mut out = String::new();
    json_stringify(&JsonValue::Object(root), &mut out);
    if fs::write(dir_path.join("settings.json"), out.as_bytes()).is_ok() { 1 } else { 0 }
}

#[no_mangle]
pub extern "C" fn qvn_profile_load(save_dir: *const c_char, config_json_str: *const c_char, out_items: *mut QvnProfileItems) -> c_int {
    if out_items.is_null() {
        return 0;
    }
    unsafe {
        (*out_items).gallery = ptr::null_mut();
        (*out_items).gallery_count = 0;
        (*out_items).achievements = ptr::null_mut();
        (*out_items).achievement_count = 0;
    }
    let dir_str = if save_dir.is_null() {
        "saves"
    } else {
        unsafe { CStr::from_ptr(save_dir) }.to_str().unwrap_or("saves")
    };
    let (gal_unlocked, ach_unlocked) = read_profile_sets(Path::new(dir_str));

    let mut gal_defs = Vec::new();
    let mut ach_defs = Vec::new();

    if !config_json_str.is_null() {
        if let Ok(cfg_text) = unsafe { CStr::from_ptr(config_json_str) }.to_str() {
            if let Some(JsonValue::Object(root)) = json_parse(cfg_text) {
                if let Some(JsonValue::Array(arr)) = root.get("gallery") {
                    for item in arr {
                        if let JsonValue::Object(m) = item {
                            let mut d = QvnGalleryDef {
                                id: [0; 64],
                                title: [0; 128],
                                image: [0; 256],
                                unlocked: 0,
                            };
                            let id = get_str(m, "id").unwrap_or("");
                            copy_str_to_buf(id, &mut d.id);
                            copy_str_to_buf(get_str(m, "title").unwrap_or(id), &mut d.title);
                            copy_str_to_buf(get_str(m, "image").unwrap_or(""), &mut d.image);
                            d.unlocked = if gal_unlocked.contains(id) { 1 } else { 0 };
                            gal_defs.push(d);
                        }
                    }
                }
                if let Some(JsonValue::Array(arr)) = root.get("achievements") {
                    for item in arr {
                        if let JsonValue::Object(m) = item {
                            let mut d = QvnAchievementDef {
                                id: [0; 64],
                                title: [0; 128],
                                description: [0; 256],
                                unlocked: 0,
                            };
                            let id = get_str(m, "id").unwrap_or("");
                            copy_str_to_buf(id, &mut d.id);
                            copy_str_to_buf(get_str(m, "title").unwrap_or(id), &mut d.title);
                            copy_str_to_buf(get_str(m, "description").unwrap_or(""), &mut d.description);
                            d.unlocked = if ach_unlocked.contains(id) { 1 } else { 0 };
                            ach_defs.push(d);
                        }
                    }
                }
            }
        }
    }

    unsafe {
        if !gal_defs.is_empty() {
            (*out_items).gallery_count = gal_defs.len();
            (*out_items).gallery = Box::into_raw(gal_defs.into_boxed_slice()) as *mut QvnGalleryDef;
        }
        if !ach_defs.is_empty() {
            (*out_items).achievement_count = ach_defs.len();
            (*out_items).achievements = Box::into_raw(ach_defs.into_boxed_slice()) as *mut QvnAchievementDef;
        }
    }
    1
}

#[no_mangle]
pub extern "C" fn qvn_profile_unlock_gallery(save_dir: *const c_char, id: *const c_char) -> c_int {
    if save_dir.is_null() || id.is_null() {
        return 0;
    }
    let dir_str = match unsafe { CStr::from_ptr(save_dir) }.to_str() {
        Ok(s) => s,
        Err(_) => return 0,
    };
    let id_str = match unsafe { CStr::from_ptr(id) }.to_str() {
        Ok(s) if !s.is_empty() => s,
        _ => return 0,
    };
    let dir = Path::new(dir_str);
    let (mut gal, ach) = read_profile_sets(dir);
    if !gal.insert(String::from(id_str)) {
        return 0;
    }
    if write_profile_sets(dir, &gal, &ach) { 1 } else { 0 }
}

#[no_mangle]
pub extern "C" fn qvn_profile_unlock_achievement(save_dir: *const c_char, id: *const c_char) -> c_int {
    if save_dir.is_null() || id.is_null() {
        return 0;
    }
    let dir_str = match unsafe { CStr::from_ptr(save_dir) }.to_str() {
        Ok(s) => s,
        Err(_) => return 0,
    };
    let id_str = match unsafe { CStr::from_ptr(id) }.to_str() {
        Ok(s) if !s.is_empty() => s,
        _ => return 0,
    };
    let dir = Path::new(dir_str);
    let (gal, mut ach) = read_profile_sets(dir);
    if !ach.insert(String::from(id_str)) {
        return 0;
    }
    if write_profile_sets(dir, &gal, &ach) { 1 } else { 0 }
}

#[no_mangle]
pub extern "C" fn qvn_profile_free(items: *mut QvnProfileItems) {
    if items.is_null() {
        return;
    }
    unsafe {
        if !(*items).gallery.is_null() && (*items).gallery_count > 0 {
            let p = ptr::slice_from_raw_parts_mut((*items).gallery, (*items).gallery_count);
            let _ = Box::from_raw(p);
            (*items).gallery = ptr::null_mut();
            (*items).gallery_count = 0;
        }
        if !(*items).achievements.is_null() && (*items).achievement_count > 0 {
            let p = ptr::slice_from_raw_parts_mut((*items).achievements, (*items).achievement_count);
            let _ = Box::from_raw(p);
            (*items).achievements = ptr::null_mut();
            (*items).achievement_count = 0;
        }
    }
}

#[no_mangle]
pub extern "C" fn qvn_locale_lookup(locale_json_str: *const c_char, key: *const c_char) -> *mut c_char {
    if locale_json_str.is_null() || key.is_null() {
        return ptr::null_mut();
    }
    let json_s = match unsafe { CStr::from_ptr(locale_json_str) }.to_str() {
        Ok(s) => s,
        Err(_) => return ptr::null_mut(),
    };
    let key_s = match unsafe { CStr::from_ptr(key) }.to_str() {
        Ok(s) => s,
        Err(_) => return ptr::null_mut(),
    };
    let map = match json_parse(json_s) {
        Some(JsonValue::Object(m)) => m,
        _ => return ptr::null_mut(),
    };
    if let Some(val) = get_str(&map, key_s) {
        if let Ok(c) = CString::new(val) {
            return c.into_raw();
        }
    }
    ptr::null_mut()
}
