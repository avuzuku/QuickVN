use std::os::raw::c_char;

#[repr(C)]
#[derive(Copy, Clone)]
pub struct QvnColor {
    pub r: u8,
    pub g: u8,
    pub b: u8,
    pub a: u8,
}

#[repr(C)]
pub struct QvnConfig {
    pub window_title: [c_char; 128],
    pub window_width: u32,
    pub window_height: u32,
    pub fps_limit: u32,
    pub fullscreen: u8,
    pub vsync: u8,

    pub font_path: [c_char; 256],
    pub font_ru_path: [c_char; 256],
    pub font_ja_path: [c_char; 256],
    pub entry_script: [c_char; 256],
    pub save_dir: [c_char; 256],
    pub default_save_slot: [c_char; 64],

    pub theme_accent_color: QvnColor,
    pub theme_accent_hover_color: QvnColor,
    pub theme_panel_color: QvnColor,
    pub theme_panel_border_color: QvnColor,
    pub theme_button_color: QvnColor,
    pub theme_button_hover_color: QvnColor,
    pub theme_corner_radius: f32,
    pub theme_shadow_alpha: u8,

    pub menu_enabled: u8,
    pub menu_title: [c_char; 128],
    pub menu_subtitle: [c_char; 256],
    pub menu_background: [c_char; 256],
    pub menu_music: [c_char; 256],
    pub menu_align: [c_char; 16],
    pub menu_overlay_color: QvnColor,
    pub menu_title_color: QvnColor,
    pub menu_subtitle_color: QvnColor,

    pub hud_enabled: u8,
    pub hud_position: [c_char; 16],
    pub hud_show_backlog: u8,
    pub hud_show_auto: u8,
    pub hud_show_skip: u8,
    pub hud_show_save: u8,
    pub hud_show_load: u8,
    pub hud_show_hide: u8,
    pub hud_show_menu: u8,

    pub box_x: f32,
    pub box_y: f32,
    pub box_width: f32,
    pub box_height: f32,
    pub box_outline_thickness: f32,
    pub box_color: QvnColor,
    pub box_outline_color: QvnColor,

    pub name_x: f32,
    pub name_y: f32,
    pub name_font_size: u32,
    pub name_color: QvnColor,

    pub text_x: f32,
    pub text_y: f32,
    pub text_font_size: u32,
    pub text_max_width: f32,
    pub text_color: QvnColor,

    pub choice_width: f32,
    pub choice_height: f32,
    pub choice_spacing: f32,
    pub choice_center_y: f32,
    pub choice_font_size: u32,
    pub choice_outline_thickness: f32,
    pub choice_bg_color: QvnColor,
    pub choice_hover_color: QvnColor,
    pub choice_outline_color: QvnColor,
    pub choice_hover_outline_color: QvnColor,
    pub choice_text_color: QvnColor,

    pub typewriter_enabled: u8,
    pub typewriter_cps: f32,
    pub typewriter_sound: [c_char; 256],
    pub typewriter_sound_interval: u32,
    pub auto_delay: f32,

    pub bg_fade_duration: f32,
    pub char_fade_duration: f32,
    pub char_move_duration: f32,
    pub default_char_x: f32,
    pub default_char_y: f32,
    pub idle_breathing: u8,
    pub vignette_strength: f32,

    pub music_volume: f32,
    pub sound_volume: f32,
    pub voice_volume: f32,

    pub hot_reload_enabled: u8,
    pub hot_reload_interval_ms: u32,
    pub mods_enabled: u8,

    pub key_advance: [c_char; 32],
    pub key_advance_alt: [c_char; 32],
    pub key_quicksave: [c_char; 32],
    pub key_quickload: [c_char; 32],
    pub key_reload: [c_char; 32],
    pub key_exit: [c_char; 32],
    pub key_backlog: [c_char; 32],
    pub key_auto_mode: [c_char; 32],
    pub key_skip_mode: [c_char; 32],
    pub key_hide_ui: [c_char; 32],

    pub game_version: [c_char; 32],
    pub game_author: [c_char; 128],
    pub gallery_enabled: u8,
    pub achievements_enabled: u8,
    pub dev_inspector_enabled: u8,
    pub single_file_saves: u8,
    pub save_slots_count: u32,
    pub backlog_limit: u32,
    pub skip_interval_ms: u32,

    pub menu_show_continue: u8,
    pub menu_show_load: u8,
    pub menu_show_gallery: u8,
    pub menu_show_achievements: u8,
    pub menu_show_mods: u8,
    pub menu_show_settings: u8,
    pub menu_show_quit: u8,
    pub menu_logo: [c_char; 256],

    pub dialogue_line_spacing: f32,
    pub nvl_bg_color: QvnColor,
    pub nvl_x: f32,
    pub nvl_y: f32,
    pub nvl_max_width: f32,
    pub nvl_max_lines: u32,

    pub dim_inactive_chars: u8,
    pub inactive_char_brightness: u8,

    pub ui_sound_choice: [c_char; 256],
    pub ui_sound_save: [c_char; 256],
    pub ui_sound_load: [c_char; 256],
    pub ui_sound_achievement: [c_char; 256],
}

#[repr(C)]
pub struct QvnPlayerSettings {
    pub music_volume: f32,
    pub sound_volume: f32,
    pub voice_volume: f32,
    pub typewriter_cps: f32,
    pub auto_delay: f32,
    pub typewriter_enabled: u8,
    pub idle_breathing: u8,
    pub res_width: u32,
    pub res_height: u32,
    pub window_mode: u8,
    pub scale_mode: u8,
    pub language: [c_char; 16],
}

#[repr(C)]
pub struct QvnSaveData {
    pub command_idx: u64,
    pub choice_history: *mut i32,
    pub choice_count: usize,
    pub js_state_json: *mut c_char,
}

#[repr(C)]
pub struct QvnSaveSlotMeta {
    pub slot: [c_char; 64],
    pub speaker: [c_char; 64],
    pub preview: [c_char; 128],
    pub timestamp: [c_char; 32],
    pub saved_epoch: u64,
    pub exists: u8,
}

#[repr(C)]
pub struct QvnGalleryDef {
    pub id: [c_char; 64],
    pub title: [c_char; 128],
    pub image: [c_char; 256],
    pub unlocked: u8,
}

#[repr(C)]
pub struct QvnAchievementDef {
    pub id: [c_char; 64],
    pub title: [c_char; 128],
    pub description: [c_char; 256],
    pub unlocked: u8,
}

#[repr(C)]
pub struct QvnProfileItems {
    pub gallery: *mut QvnGalleryDef,
    pub gallery_count: usize,
    pub achievements: *mut QvnAchievementDef,
    pub achievement_count: usize,
}

#[repr(C)]
#[derive(Copy, Clone)]
pub struct QvnModInfo {
    pub id: [c_char; 128],
    pub name: [c_char; 128],
    pub version: [c_char; 32],
    pub author: [c_char; 128],
    pub description: [c_char; 256],
    pub root_path: [c_char; 512],
    pub hook_script: [c_char; 256],
    pub config_patch: [c_char; 256],
    pub is_engine_mod: u8,
    pub enabled: u8,
}

#[repr(C)]
pub struct QvnModList {
    pub items: *mut QvnModInfo,
    pub count: usize,
}

#[repr(C)]
pub struct QvnBytes {
    pub data: *mut u8,
    pub len: usize,
}

#[repr(C)]
#[derive(Copy, Clone)]
pub struct QvnBuildTarget {
    pub id: [c_char; 32],
    pub build_cmd: [c_char; 512],
    pub game_bin: [c_char; 256],
    pub out_exe: [c_char; 128],
    pub enabled: u8,
}

#[repr(C)]
pub struct QvnLauncherConfig {
    pub proj_root: [c_char; 256],
    pub active_project: [c_char; 128],
    pub editor_command: [c_char; 256],
    pub pak_key: [c_char; 128],
    pub runtimes_dir: [c_char; 256],
    pub studio_lang: [c_char; 8],
    pub studio_fps_limit: u32,
    pub auto_open_dist: u8,
    pub default_template: u8,
    pub targets: [QvnBuildTarget; 5],
    pub target_count: usize,
}

#[repr(C)]
pub struct QvnProjectEntry {
    pub name: [c_char; 128],
    pub dir: [c_char; 256],
}

#[repr(C)]
pub struct QvnProjectList {
    pub items: *mut QvnProjectEntry,
    pub count: usize,
}

pub fn copy_str_to_buf(s: &str, buf: &mut [c_char]) {
    if buf.is_empty() {
        return;
    }
    for b in buf.iter_mut() {
        *b = 0;
    }
    let src = s.as_bytes();
    let max_len = buf.len() - 1;
    let copy_len = src.len().min(max_len);
    for i in 0..copy_len {
        buf[i] = src[i] as c_char;
    }
    buf[copy_len] = 0;
}

pub fn buf_to_string(buf: &[c_char]) -> String {
    let mut bytes = Vec::new();
    for &c in buf {
        if c == 0 {
            break;
        }
        bytes.push(c as u8);
    }
    String::from_utf8_lossy(&bytes).into_owned()
}