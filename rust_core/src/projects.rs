use crate::json::{escape_json_str, get_bool, get_obj, get_str, get_u32, get_u8, json_parse, json_stringify, JsonValue};
use crate::mods::create_test_mod_internal;
use crate::types::{
    buf_to_string, copy_str_to_buf, QvnBuildTarget, QvnLauncherConfig, QvnProjectEntry,
    QvnProjectList,
};
use std::collections::BTreeMap;
use std::ffi::CStr;
use std::fs;
use std::os::raw::{c_char, c_int};
use std::path::{Path, PathBuf};
use std::ptr;

const DEFAULT_EN_LOCALE: &str = r#"{
  "new_game": "New Game",
  "continue": "Continue",
  "load_game": "Load Game",
  "save_game": "Save Game",
  "gallery": "Gallery",
  "achievements": "Achievements",
  "mods": "Mods",
  "settings": "Settings",
  "quit": "Exit",
  "resume": "Resume",
  "main_menu": "Main Menu",
  "paused": "Game Paused",
  "done": "Done",
  "empty_slot": "Empty Slot",
  "music_volume": "Music Volume",
  "sound_volume": "Sound Volume",
  "voice_volume": "Voice Volume",
  "text_speed": "Text Speed",
  "auto_delay": "Auto Delay",
  "resolution": "Resolution",
  "window_mode": "Display Mode",
  "scale_mode": "Aspect Scaling",
  "scale_fit": "Fit (Keep Ratio)",
  "scale_fill": "Fill (Full Window)",
  "language": "Language",
  "mode_windowed": "Windowed",
  "mode_borderless": "Borderless",
  "mode_fullscreen": "Fullscreen",
  "typewriter": "Typewriter Effect",
  "idle_breathing": "Character Breathing",
  "history_title": "Dialogue History",
  "the_end": "The End",
  "ending_sub": "Thank you for playing",
  "locked": "Locked",
  "unlocked": "Unlocked",
  "quicksaved": "Quicksaved",
  "autosaved": "Autosaved",
  "loaded": "Loaded",
  "no_saves": "No saves found",
  "enter_name_title": "What is your name?",
  "enter_name_placeholder": "Type your name here...",
  "hud_log": "Log",
  "hud_auto": "Auto",
  "hud_skip": "Skip",
  "hud_save": "Save",
  "hud_load": "Load",
  "hud_hide": "Hide",
  "hud_menu": "Menu",
  "mods_title": "Mod Manager",
  "mods_subtitle": "Toggle mods and arrange load order inside the game",
  "mods_empty": "No mods installed",
  "mods_empty_hint": "Place mod folders inside ./mods/ or ./engine_mods/",
  "mods_rescan": "Rescan",
  "mods_apply": "Apply & Reload",
  "mods_applied": "Mods applied",
  "mods_create_demo": "Create Test Mod",
  "mods_demo_created": "Test mod created",
  "mods_engine": "ENGINE",
  "mods_game": "GAME"
}"#;

const DEFAULT_RU_LOCALE: &str = r#"{
  "new_game": "Новая игра",
  "continue": "Продолжить",
  "load_game": "Загрузить",
  "save_game": "Сохранить",
  "gallery": "Галерея",
  "achievements": "Достижения",
  "mods": "Моды",
  "settings": "Настройки",
  "quit": "Выход",
  "resume": "Продолжить",
  "main_menu": "Главное меню",
  "paused": "Пауза",
  "done": "Готово",
  "empty_slot": "Пустой слот",
  "music_volume": "Громкость музыки",
  "sound_volume": "Громкость звуков",
  "voice_volume": "Громкость голоса",
  "text_speed": "Скорость текста",
  "auto_delay": "Задержка авто",
  "resolution": "Разрешение",
  "window_mode": "Режим экрана",
  "scale_mode": "Масштаб экрана",
  "scale_fit": "По пропорциям",
  "scale_fill": "На весь экран",
  "language": "Язык",
  "mode_windowed": "Оконный",
  "mode_borderless": "Без рамок",
  "mode_fullscreen": "Полный экран",
  "typewriter": "Эффект печати",
  "idle_breathing": "Дыхание спрайтов",
  "history_title": "История диалогов",
  "the_end": "Конец",
  "ending_sub": "Спасибо за игру",
  "locked": "Закрыто",
  "unlocked": "Открыто",
  "quicksaved": "Быстрое сохранение",
  "autosaved": "Автосохранение",
  "loaded": "Загружено",
  "no_saves": "Сохранения не найдены",
  "enter_name_title": "Как тебя зовут?",
  "enter_name_placeholder": "Введи имя персонажа...",
  "hud_log": "Лог",
  "hud_auto": "Авто",
  "hud_skip": "Скип",
  "hud_save": "Сохр",
  "hud_load": "Загр",
  "hud_hide": "Скр",
  "hud_menu": "Меню",
  "mods_title": "Менеджер модов",
  "mods_subtitle": "Включайте моды и меняйте порядок загрузки прямо в игре",
  "mods_empty": "Моды не найдены",
  "mods_empty_hint": "Поместите папки модов в ./mods/ или ./engine_mods/",
  "mods_rescan": "Обновить",
  "mods_apply": "Применить",
  "mods_applied": "Моды применены",
  "mods_create_demo": "Создать тест-мод",
  "mods_demo_created": "Тест-мод создан",
  "mods_engine": "ДВИЖОК",
  "mods_game": "ИГРА"
}"#;

const DEFAULT_JA_LOCALE: &str = r#"{
  "new_game": "ニューゲーム",
  "continue": "つづきから",
  "load_game": "ロード",
  "save_game": "セーブ",
  "gallery": "ギャラリー",
  "achievements": "実績",
  "mods": "MOD",
  "settings": "設定",
  "quit": "終了",
  "resume": "再開",
  "main_menu": "タイトルへ",
  "paused": "ポーズ",
  "done": "完了",
  "empty_slot": "空きスロット",
  "music_volume": "BGM音量",
  "sound_volume": "SE音量",
  "voice_volume": "ボイス音量",
  "text_speed": "文字表示速度",
  "auto_delay": "オート待機",
  "resolution": "解像度",
  "window_mode": "画面モード",
  "scale_mode": "アスペクト比",
  "scale_fit": "比率維持",
  "scale_fill": "全画面フィット",
  "language": "言語",
  "mode_windowed": "ウィンドウ",
  "mode_borderless": "ボーダーレス",
  "mode_fullscreen": "フルスクリーン",
  "typewriter": "タイプライター表示",
  "idle_breathing": "立ち絵呼吸モーション",
  "history_title": "バックログ",
  "the_end": "終",
  "ending_sub": "プレイありがとうございました",
  "locked": "未開放",
  "unlocked": "開放済み",
  "quicksaved": "クイックセーブしました",
  "autosaved": "オートセーブしました",
  "loaded": "ロードしました",
  "no_saves": "セーブデータがありません",
  "enter_name_title": "君の名前は？",
  "enter_name_placeholder": "名前を入力してください...",
  "hud_log": "履歴",
  "hud_auto": "自動",
  "hud_skip": "早送",
  "hud_save": "保存",
  "hud_load": "読込",
  "hud_hide": "非表示",
  "hud_menu": "MENU",
  "mods_title": "MOD管理",
  "mods_subtitle": "ゲーム内でMODの有効化と読み込み順を変更できます",
  "mods_empty": "MODが見つかりません",
  "mods_empty_hint": "./mods/ または ./engine_mods/ に配置してください",
  "mods_rescan": "再読込",
  "mods_apply": "適用して再読込",
  "mods_applied": "MODを適用しました",
  "mods_create_demo": "テストMOD作成",
  "mods_demo_created": "テストMODを作成しました",
  "mods_engine": "ENGINE",
  "mods_game": "GAME"
}"#;

fn empty_build_target() -> QvnBuildTarget {
    QvnBuildTarget {
        id: [0; 32],
        build_cmd: [0; 512],
        game_bin: [0; 256],
        out_exe: [0; 128],
        enabled: 0,
    }
}

fn set_default_launcher_config(cfg: &mut QvnLauncherConfig) {
    copy_str_to_buf("proj", &mut cfg.proj_root);
    copy_str_to_buf("", &mut cfg.active_project);
    copy_str_to_buf("", &mut cfg.editor_command);
    copy_str_to_buf("quickvn-default-secret-key-2026", &mut cfg.pak_key);
    copy_str_to_buf("runtimes", &mut cfg.runtimes_dir);
    copy_str_to_buf("en", &mut cfg.studio_lang);
    cfg.studio_fps_limit = 60;
    cfg.auto_open_dist = 0;
    cfg.default_template = 0;

    let defaults: [(&str, &str, &str, &str, u8); 5] = [
        (
            "linux",
            "cmake -B /tmp/quickvn_build/linux -DCMAKE_BUILD_TYPE=Release && cmake --build /tmp/quickvn_build/linux --target quickjs_vn -j",
            "runtimes/linux/quickjs_vn",
            "game_linux",
            1,
        ),
        (
            "windows",
            "cmake -B /tmp/quickvn_build/windows -DCMAKE_TOOLCHAIN_FILE=toolchains/mingw-w64.cmake -DCMAKE_BUILD_TYPE=Release && cmake --build /tmp/quickvn_build/windows --target quickjs_vn -j",
            "runtimes/windows/quickjs_vn.exe",
            "game_windows.exe",
            0,
        ),
        (
            "macos",
            "cmake -B /tmp/quickvn_build/macos -DCMAKE_TOOLCHAIN_FILE=toolchains/macos.cmake -DCMAKE_BUILD_TYPE=Release && cmake --build /tmp/quickvn_build/macos --target quickjs_vn -j",
            "runtimes/macos/quickjs_vn",
            "game_macos",
            0,
        ),
        (
            "android",
            "cmake -B /tmp/quickvn_build/android -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK/build/cmake/android.toolchain.cmake -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-24 -DQVN_BUILD_STUDIO=OFF && cmake --build /tmp/quickvn_build/android --target quickjs_vn -j",
            "runtimes/android/libquickjs_vn.so",
            "libquickjs_vn.so",
            0,
        ),
        (
            "ios",
            "cmake -B /tmp/quickvn_build/ios -G Xcode -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_SYSROOT=iphoneos -DQVN_BUILD_STUDIO=OFF && cmake --build /tmp/quickvn_build/ios --target quickjs_vn --config Release",
            "runtimes/ios/quickjs_vn",
            "game_ios",
            0,
        ),
    ];

    cfg.target_count = 5;
    for (i, d) in defaults.iter().enumerate() {
        let mut t = empty_build_target();
        copy_str_to_buf(d.0, &mut t.id);
        copy_str_to_buf(d.1, &mut t.build_cmd);
        copy_str_to_buf(d.2, &mut t.game_bin);
        copy_str_to_buf(d.3, &mut t.out_exe);
        t.enabled = d.4;
        cfg.targets[i] = t;
    }
}

#[no_mangle]
pub extern "C" fn qvn_launcher_config_default(out_cfg: *mut QvnLauncherConfig) {
    if out_cfg.is_null() {
        return;
    }
    unsafe {
        set_default_launcher_config(&mut *out_cfg);
    }
}

#[no_mangle]
pub extern "C" fn qvn_launcher_config_load(path: *const c_char, out_cfg: *mut QvnLauncherConfig) -> c_int {
    if out_cfg.is_null() {
        return 0;
    }
    unsafe {
        set_default_launcher_config(&mut *out_cfg);
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
    let root = match json_parse(&content) {
        Some(v) => v,
        None => return 0,
    };
    let map = match &root {
        JsonValue::Object(m) => m,
        _ => return 0,
    };

    unsafe {
        let cfg = &mut *out_cfg;
        if let Some(v) = get_str(map, "proj_root") { copy_str_to_buf(v, &mut cfg.proj_root); }
        if let Some(v) = get_str(map, "active_project") { copy_str_to_buf(v, &mut cfg.active_project); }
        if let Some(v) = get_str(map, "editor_command") { copy_str_to_buf(v, &mut cfg.editor_command); }
        if let Some(v) = get_str(map, "pak_key") { copy_str_to_buf(v, &mut cfg.pak_key); }
        if let Some(v) = get_str(map, "runtimes_dir") { copy_str_to_buf(v, &mut cfg.runtimes_dir); }
        if let Some(v) = get_str(map, "studio_lang") { copy_str_to_buf(v, &mut cfg.studio_lang); }
        if let Some(v) = get_u32(map, "studio_fps_limit") { cfg.studio_fps_limit = v.clamp(30, 240); }
        if let Some(v) = get_bool(map, "auto_open_dist") { cfg.auto_open_dist = v; }
        if let Some(v) = get_u8(map, "default_template") { cfg.default_template = v.min(2); }

        if let Some(targets_map) = get_obj(&root, "targets") {
            let names = ["linux", "windows", "macos", "android", "ios"];
            for (i, tname) in names.iter().enumerate() {
                if let Some(JsonValue::Object(tm)) = targets_map.get(*tname) {
                    if let Some(v) = get_bool(tm, "enabled") { cfg.targets[i].enabled = v; }
                    if let Some(v) = get_str(tm, "build_cmd") { copy_str_to_buf(v, &mut cfg.targets[i].build_cmd); }
                    if let Some(v) = get_str(tm, "game_bin") { copy_str_to_buf(v, &mut cfg.targets[i].game_bin); }
                    if let Some(v) = get_str(tm, "out_exe") { copy_str_to_buf(v, &mut cfg.targets[i].out_exe); }
                }
            }
        }
    }
    1
}

#[no_mangle]
pub extern "C" fn qvn_launcher_config_save(path: *const c_char, cfg_ptr: *const QvnLauncherConfig) -> c_int {
    if path.is_null() || cfg_ptr.is_null() {
        return 0;
    }
    let path_str = match unsafe { CStr::from_ptr(path) }.to_str() {
        Ok(s) if !s.is_empty() => s,
        _ => return 0,
    };
    let mut root = BTreeMap::new();
    let mut targets_map = BTreeMap::new();
    unsafe {
        let cfg = &*cfg_ptr;
        root.insert(String::from("proj_root"), JsonValue::Str(buf_to_string(&cfg.proj_root)));
        root.insert(String::from("active_project"), JsonValue::Str(buf_to_string(&cfg.active_project)));
        root.insert(String::from("editor_command"), JsonValue::Str(buf_to_string(&cfg.editor_command)));
        root.insert(String::from("pak_key"), JsonValue::Str(buf_to_string(&cfg.pak_key)));
        root.insert(String::from("runtimes_dir"), JsonValue::Str(buf_to_string(&cfg.runtimes_dir)));
        root.insert(String::from("studio_lang"), JsonValue::Str(buf_to_string(&cfg.studio_lang)));
        root.insert(String::from("studio_fps_limit"), JsonValue::Number(cfg.studio_fps_limit as f64));
        root.insert(String::from("auto_open_dist"), JsonValue::Bool(cfg.auto_open_dist != 0));
        root.insert(String::from("default_template"), JsonValue::Number(cfg.default_template as f64));

        for i in 0..cfg.target_count.min(5) {
            let t = &cfg.targets[i];
            let tid = buf_to_string(&t.id);
            let mut tm = BTreeMap::new();
            tm.insert(String::from("enabled"), JsonValue::Bool(t.enabled != 0));
            tm.insert(String::from("build_cmd"), JsonValue::Str(buf_to_string(&t.build_cmd)));
            tm.insert(String::from("game_bin"), JsonValue::Str(buf_to_string(&t.game_bin)));
            tm.insert(String::from("out_exe"), JsonValue::Str(buf_to_string(&t.out_exe)));
            targets_map.insert(tid, JsonValue::Object(tm));
        }
    }
    root.insert(String::from("targets"), JsonValue::Object(targets_map));

    let mut out = String::new();
    json_stringify(&JsonValue::Object(root), &mut out);
    if fs::write(path_str, out.as_bytes()).is_ok() { 1 } else { 0 }
}

#[no_mangle]
pub extern "C" fn qvn_projects_scan(proj_root: *const c_char, out_list: *mut QvnProjectList) -> c_int {
    if out_list.is_null() {
        return 0;
    }
    unsafe {
        (*out_list).items = ptr::null_mut();
        (*out_list).count = 0;
    }
    let root_str = if proj_root.is_null() {
        "proj"
    } else {
        unsafe { CStr::from_ptr(proj_root) }.to_str().unwrap_or("proj")
    };
    let root_path = Path::new(root_str);
    let _ = fs::create_dir_all(root_path);

    let mut projects: Vec<QvnProjectEntry> = Vec::new();
    if let Ok(entries) = fs::read_dir(root_path) {
        let mut folders: Vec<PathBuf> = entries.flatten().map(|e| e.path()).filter(|p| p.is_dir()).collect();
        folders.sort();
        for f in folders {
            if f.join("config.json").is_file() {
                if let Some(fname) = f.file_name().and_then(|s| s.to_str()) {
                    let mut entry = QvnProjectEntry {
                        name: [0; 128],
                        dir: [0; 256],
                    };
                    copy_str_to_buf(fname, &mut entry.name);
                    if let Some(full) = f.to_str() {
                        copy_str_to_buf(full, &mut entry.dir);
                    }
                    projects.push(entry);
                }
            }
        }
    }

    let count = projects.len();
    if count > 0 {
        unsafe {
            (*out_list).count = count;
            (*out_list).items = Box::into_raw(projects.into_boxed_slice()) as *mut QvnProjectEntry;
        }
    }
    1
}

#[no_mangle]
pub extern "C" fn qvn_projects_free(list: *mut QvnProjectList) {
    if list.is_null() {
        return;
    }
    unsafe {
        if !(*list).items.is_null() && (*list).count > 0 {
            let p = ptr::slice_from_raw_parts_mut((*list).items, (*list).count);
            let _ = Box::from_raw(p);
            (*list).items = ptr::null_mut();
            (*list).count = 0;
        }
    }
}

#[no_mangle]
pub extern "C" fn qvn_create_project(
    font_src: *const c_char,
    target_dir: *const c_char,
    game_name: *const c_char,
    with_test_mod: c_int,
    template_id: c_int,
) -> c_int {
    if target_dir.is_null() || game_name.is_null() {
        return 0;
    }
    let dst_str = match unsafe { CStr::from_ptr(target_dir) }.to_str() {
        Ok(s) if !s.is_empty() => s,
        _ => return 0,
    };
    let name_str = match unsafe { CStr::from_ptr(game_name) }.to_str() {
        Ok(s) if !s.is_empty() => s,
        _ => return 0,
    };
    let font_str = if font_src.is_null() {
        ""
    } else {
        unsafe { CStr::from_ptr(font_src) }.to_str().unwrap_or("")
    };

    let dst = Path::new(dst_str);

    let subdirs = [
        "assets/bg",
        "assets/chars",
        "assets/ui",
        "assets/fonts",
        "assets/audio/music",
        "assets/audio/sfx",
        "assets/audio/voice",
        "scripts",
        "locales",
        "mods",
        "engine_mods",
        "saves",
    ];
    for sub in &subdirs {
        if fs::create_dir_all(dst.join(sub)).is_err() {
            return 0;
        }
    }

    let _ = fs::write(dst.join("assets/fonts/font.ttf"), crate::BUILTIN_FONT);
    if !font_str.is_empty() && !dst.join("assets/fonts/font.ttf").is_file() {
        let src_p = Path::new(font_str);
        if src_p.is_file() {
            let _ = fs::copy(src_p, dst.join("assets/fonts/font.ttf"));
        }
    }

    let _ = fs::write(dst.join("locales/en.json"), DEFAULT_EN_LOCALE.as_bytes());
    let _ = fs::write(dst.join("locales/ru.json"), DEFAULT_RU_LOCALE.as_bytes());
    let _ = fs::write(dst.join("locales/ja.json"), DEFAULT_JA_LOCALE.as_bytes());

    let mut esc_name = String::new();
    escape_json_str(name_str, &mut esc_name);

    let entry_script = if template_id == 1 { "scripts/main.rpy" } else { "scripts/main.js" };
    let cfg_json = format!(
        "{{\n  \"window\": {{ \"title\": {0}, \"width\": 1280, \"height\": 720, \"fps_limit\": 60, \"fullscreen\": false, \"vsync\": true }},\n  \"paths\": {{ \"font\": \"assets/fonts/font.ttf\", \"font_ru\": \"\", \"font_ja\": \"\", \"entry_script\": \"{1}\", \"save_dir\": \"saves\", \"default_save_slot\": \"quicksave\" }},\n  \"features\": {{ \"mods_enabled\": true }},\n  \"main_menu\": {{ \"enabled\": true, \"title\": {0}, \"subtitle\": \"\", \"background\": \"\", \"music\": \"\", \"align\": \"left\" }},\n  \"hot_reload\": {{ \"enabled\": true, \"interval_ms\": 450 }}\n}}\n",
        esc_name, entry_script
    );
    if fs::write(dst.join("config.json"), cfg_json.as_bytes()).is_err() {
        return 0;
    }

    let safe_js = name_str.replace('\\', "\\\\").replace('"', "\\\"");
    if template_id == 1 {
        let starter_rpy = format!(
            "define n_char = Character(None)\n\nlabel start:\n    chapter \"Chapter 1\" \"{0}\"\n    weather sakura\n    \"Welcome to {0} (Ren'Py .rpy mode)!\"\n    ending \"The End\" \"Demo complete\"\n",
            safe_js
        );
        if fs::write(dst.join("scripts/main.rpy"), starter_rpy.as_bytes()).is_err() {
            return 0;
        }
    } else if template_id == 2 {
        if fs::write(dst.join("scripts/main.js"), b"n(\"New blank project ready.\");\n").is_err() {
            return 0;
        }
    } else {
        let starter_js = format!(
            "chapter(\"Chapter 1\", \"{0}\", 2.0);\nweather(\"sakura\", 35);\nn(\"Welcome to {0}! Assets are organized into assets/bg, assets/chars, assets/fonts and assets/audio/(music, sfx, voice).\");\nnvl(true);\nn(\"Open the in-game Mods menu to test Neon Violet Theme.\");\nnvl(false);\nending(\"The End\", \"Demo complete\");\n",
            safe_js
        );
        if fs::write(dst.join("scripts/main.js"), starter_js.as_bytes()).is_err() {
            return 0;
        }
    }

    if with_test_mod != 0 {
        let _ = create_test_mod_internal(dst);
    }

    1
}