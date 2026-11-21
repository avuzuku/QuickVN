use crate::json::{get_bool, get_str, json_parse, json_stringify, JsonValue};
use crate::types::{buf_to_string, copy_str_to_buf, QvnModInfo, QvnModList};
use std::collections::BTreeMap;
use std::ffi::CStr;
use std::fs;
use std::os::raw::{c_char, c_int};
use std::path::{Path, PathBuf};
use std::ptr;
use std::slice;

fn empty_mod_info() -> QvnModInfo {
    QvnModInfo {
        id: [0; 128],
        name: [0; 128],
        version: [0; 32],
        author: [0; 128],
        description: [0; 256],
        root_path: [0; 512],
        hook_script: [0; 256],
        config_patch: [0; 256],
        is_engine_mod: 0,
        enabled: 0,
    }
}

fn scan_mods_dir(dir: &Path, is_engine: bool, out: &mut Vec<QvnModInfo>) {
    let entries = match fs::read_dir(dir) {
        Ok(e) => e,
        Err(_) => return,
    };
    let mut folders: Vec<PathBuf> = entries
        .flatten()
        .map(|e| e.path())
        .filter(|p| p.is_dir())
        .collect();
    folders.sort();

    for folder in folders {
        let folder_name = match folder.file_name().and_then(|s| s.to_str()) {
            Some(n) => n,
            None => continue,
        };
        let mut info = empty_mod_info();
        copy_str_to_buf(folder_name, &mut info.id);
        copy_str_to_buf(folder_name, &mut info.name);
        copy_str_to_buf("1.0.0", &mut info.version);
        copy_str_to_buf("Unknown", &mut info.author);
        copy_str_to_buf("", &mut info.description);
        if let Some(rp) = folder.to_str() {
            copy_str_to_buf(rp, &mut info.root_path);
        }
        info.is_engine_mod = if is_engine { 1 } else { 0 };
        info.enabled = 0;

        let default_hook = if is_engine { "init.js" } else { "patch.js" };
        if folder.join(default_hook).is_file() {
            copy_str_to_buf(default_hook, &mut info.hook_script);
        }
        if folder.join("config_patch.json").is_file() {
            copy_str_to_buf("config_patch.json", &mut info.config_patch);
        } else if folder.join("config.json").is_file() {
            copy_str_to_buf("config.json", &mut info.config_patch);
        }

        let manifest_path = folder.join("mod.json");
        if let Ok(content) = fs::read_to_string(manifest_path) {
            if let Some(JsonValue::Object(m)) = json_parse(&content) {
                if let Some(v) = get_str(&m, "name") { copy_str_to_buf(v, &mut info.name); }
                if let Some(v) = get_str(&m, "version") { copy_str_to_buf(v, &mut info.version); }
                if let Some(v) = get_str(&m, "author") { copy_str_to_buf(v, &mut info.author); }
                if let Some(v) = get_str(&m, "description") { copy_str_to_buf(v, &mut info.description); }
                if let Some(v) = get_str(&m, "hook_script") { copy_str_to_buf(v, &mut info.hook_script); }
                if let Some(v) = get_str(&m, "config_patch") { copy_str_to_buf(v, &mut info.config_patch); }
                if let Some(v) = get_str(&m, "type") {
                    if v == "engine" {
                        info.is_engine_mod = 1;
                    } else if v == "game" {
                        info.is_engine_mod = 0;
                    }
                }
            }
        }
        out.push(info);
    }
}

pub fn scan_all_mods(base_dir: &Path) -> Vec<QvnModInfo> {
    let mut discovered = Vec::new();
    scan_mods_dir(&base_dir.join("engine_mods"), true, &mut discovered);
    scan_mods_dir(&base_dir.join("mods"), false, &mut discovered);

    let manifest_path = base_dir.join("mods.json");
    let content = match fs::read_to_string(manifest_path) {
        Ok(s) => s,
        Err(_) => return discovered,
    };
    let root = match json_parse(&content) {
        Some(JsonValue::Object(m)) => m,
        _ => return discovered,
    };
    let saved_arr = match root.get("mods") {
        Some(JsonValue::Array(a)) => a,
        _ => return discovered,
    };

    let mut ordered = Vec::new();
    let mut used = vec![false; discovered.len()];

    for entry in saved_arr {
        if let JsonValue::Object(m) = entry {
            let id = match get_str(m, "id") {
                Some(s) => s,
                None => continue,
            };
            let scope_engine = get_str(m, "scope").map(|s| s == "engine").unwrap_or(false);
            let enabled = get_bool(m, "enabled").unwrap_or(0);

            for (idx, item) in discovered.iter().enumerate() {
                if !used[idx]
                    && buf_to_string(&item.id) == id
                    && (item.is_engine_mod != 0) == scope_engine
                {
                    let mut copy = *item;
                    copy.enabled = enabled;
                    ordered.push(copy);
                    used[idx] = true;
                    break;
                }
            }
        }
    }

    for (idx, item) in discovered.into_iter().enumerate() {
        if !used[idx] {
            ordered.push(item);
        }
    }

    ordered
}

pub fn create_test_mod_internal(project_dir: &Path) -> bool {
    let mod_dir = project_dir.join("engine_mods").join("neon_violet");
    if fs::create_dir_all(&mod_dir).is_err() {
        return false;
    }
    let manifest = r#"{
  "name": "Neon Violet Theme",
  "version": "1.0.0",
  "author": "QuickVN",
  "description": "Applies a neon violet UI palette and triggers an in-game verification notification.",
  "type": "engine",
  "config_patch": "config.json",
  "hook_script": "init.js"
}"#;
    let cfg_patch = r#"{
  "theme": {
    "accent_color": [168, 85, 247, 255],
    "accent_hover_color": [192, 132, 252, 255],
    "panel_color": [18, 12, 30, 244],
    "panel_border_color": [192, 132, 252, 90]
  },
  "dialogue_box": {
    "color": [22, 12, 38, 236],
    "outline_color": [192, 132, 252, 180],
    "name_color": [216, 180, 254, 255]
  },
  "choices": {
    "bg_color": [28, 16, 48, 236],
    "hover_color": [52, 28, 88, 248],
    "hover_outline_color": [192, 132, 252, 230]
  }
}"#;
    let hook = "notify(\"Mod Active: Neon Violet Theme\");\n";

    if fs::write(mod_dir.join("mod.json"), manifest.as_bytes()).is_err() {
        return false;
    }
    if fs::write(mod_dir.join("config.json"), cfg_patch.as_bytes()).is_err() {
        return false;
    }
    if fs::write(mod_dir.join("init.js"), hook.as_bytes()).is_err() {
        return false;
    }

    let mods_json = project_dir.join("mods.json");
    if !mods_json.is_file() {
        let _ = fs::write(
            mods_json,
            b"{\"mods\":[{\"enabled\":false,\"id\":\"neon_violet\",\"scope\":\"engine\"}]}",
        );
    }
    true
}

#[no_mangle]
pub extern "C" fn qvn_mods_scan(base_dir: *const c_char, out_list: *mut QvnModList) -> c_int {
    if out_list.is_null() {
        return 0;
    }
    unsafe {
        (*out_list).items = ptr::null_mut();
        (*out_list).count = 0;
    }
    let base = if base_dir.is_null() {
        PathBuf::from(".")
    } else {
        match unsafe { CStr::from_ptr(base_dir) }.to_str() {
            Ok(s) if !s.is_empty() => PathBuf::from(s),
            _ => PathBuf::from("."),
        }
    };

    let mods = scan_all_mods(&base);
    let count = mods.len();
    if count > 0 {
        let boxed = mods.into_boxed_slice();
        unsafe {
            (*out_list).items = Box::into_raw(boxed) as *mut QvnModInfo;
            (*out_list).count = count;
        }
    }
    1
}

#[no_mangle]
pub extern "C" fn qvn_mods_save(base_dir: *const c_char, items: *const QvnModInfo, count: usize) -> c_int {
    let base = if base_dir.is_null() {
        PathBuf::from(".")
    } else {
        match unsafe { CStr::from_ptr(base_dir) }.to_str() {
            Ok(s) if !s.is_empty() => PathBuf::from(s),
            _ => PathBuf::from("."),
        }
    };

    let mut arr = Vec::new();
    if !items.is_null() && count > 0 {
        let slice_items = unsafe { slice::from_raw_parts(items, count) };
        for item in slice_items {
            let mut obj = BTreeMap::new();
            obj.insert(String::from("id"), JsonValue::Str(buf_to_string(&item.id)));
            obj.insert(
                String::from("scope"),
                JsonValue::Str(String::from(if item.is_engine_mod != 0 { "engine" } else { "game" })),
            );
            obj.insert(String::from("enabled"), JsonValue::Bool(item.enabled != 0));
            arr.push(JsonValue::Object(obj));
        }
    }

    let mut root = BTreeMap::new();
    root.insert(String::from("mods"), JsonValue::Array(arr));

    let mut out = String::new();
    json_stringify(&JsonValue::Object(root), &mut out);

    let path = base.join("mods.json");
    if fs::write(path, out.as_bytes()).is_ok() {
        1
    } else {
        0
    }
}

#[no_mangle]
pub extern "C" fn qvn_mods_free(list: *mut QvnModList) {
    if list.is_null() {
        return;
    }
    unsafe {
        if !(*list).items.is_null() && (*list).count > 0 {
            let ptr_slice = ptr::slice_from_raw_parts_mut((*list).items, (*list).count);
            let _ = Box::from_raw(ptr_slice);
            (*list).items = ptr::null_mut();
            (*list).count = 0;
        }
    }
}

#[no_mangle]
pub extern "C" fn qvn_create_game_mod(project_dir: *const c_char, mod_name: *const c_char) -> c_int {
    let base = if project_dir.is_null() {
        PathBuf::from(".")
    } else {
        match unsafe { CStr::from_ptr(project_dir) }.to_str() {
            Ok(s) if !s.is_empty() => PathBuf::from(s),
            _ => PathBuf::from("."),
        }
    };
    let name_raw = if mod_name.is_null() {
        "custom_mod"
    } else {
        unsafe { CStr::from_ptr(mod_name) }.to_str().unwrap_or("custom_mod")
    };
    let mut id: String = name_raw
        .chars()
        .filter(|c| c.is_ascii_alphanumeric() || *c == '_' || *c == '-')
        .collect();
    if id.is_empty() {
        id = String::from("custom_mod");
    }
    let mod_dir = base.join("mods").join(&id);
    if fs::create_dir_all(mod_dir.join("assets")).is_err() {
        return 0;
    }
    let manifest = format!(
        "{{\n  \"name\": \"{0}\",\n  \"version\": \"1.0.0\",\n  \"author\": \"Developer\",\n  \"description\": \"Custom game content mod (overrides scripts/assets).\",\n  \"type\": \"game\",\n  \"hook_script\": \"patch.js\"\n}}\n",
        name_raw.replace('"', "\\\"")
    );
    let patch_js = format!("notify(\"Game Mod Loaded: {0}\");\n", name_raw.replace('"', "\\\""));
    if fs::write(mod_dir.join("mod.json"), manifest.as_bytes()).is_err() {
        return 0;
    }
    if fs::write(mod_dir.join("patch.js"), patch_js.as_bytes()).is_err() {
        return 0;
    }
    1
}

#[no_mangle]
pub extern "C" fn qvn_create_test_mod(project_dir: *const c_char) -> c_int {
    let base = if project_dir.is_null() {
        PathBuf::from(".")
    } else {
        match unsafe { CStr::from_ptr(project_dir) }.to_str() {
            Ok(s) if !s.is_empty() => PathBuf::from(s),
            _ => PathBuf::from("."),
        }
    };
    if create_test_mod_internal(&base) { 1 } else { 0 }
}