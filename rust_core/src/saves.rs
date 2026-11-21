use crate::json::{get_str, get_u64, json_parse, json_stringify, JsonValue};
use crate::pak::{chacha20_xor, derive_chacha_key, fnv1a_update};
use crate::types::{copy_str_to_buf, QvnSaveData, QvnSaveSlotMeta};
use std::collections::BTreeMap;
use std::ffi::{CStr, CString};
use std::fs;
use std::os::raw::{c_char, c_int};
use std::path::{Path, PathBuf};
use std::ptr;
use std::slice;
use std::sync::atomic::{AtomicBool, Ordering};
use std::time::{SystemTime, UNIX_EPOCH};

const SAVE_BUNDLE_MAGIC: &[u8; 8] = b"QVNSAV01";
const SAVE_BUNDLE_KEY: &str = "qvn-mobile-save-bundle-key-2026";

static SINGLE_FILE_SAVES: AtomicBool = AtomicBool::new(cfg!(any(target_os = "android", target_os = "ios")));

pub fn is_single_file_saves() -> bool {
    SINGLE_FILE_SAVES.load(Ordering::Relaxed)
}

#[no_mangle]
pub extern "C" fn qvn_saves_set_single_file(enabled: c_int) {
    if cfg!(any(target_os = "android", target_os = "ios")) {
        SINGLE_FILE_SAVES.store(true, Ordering::Relaxed);
    } else {
        SINGLE_FILE_SAVES.store(enabled != 0, Ordering::Relaxed);
    }
}

pub fn read_save_bundle(dir: &Path) -> BTreeMap<String, JsonValue> {
    let path = dir.join("savedata.qvn");
    let raw = match fs::read(path) {
        Ok(b) => b,
        Err(_) => return BTreeMap::new(),
    };
    let text = if raw.len() >= 20 && &raw[0..8] == SAVE_BUNDLE_MAGIC {
        let mut nonce = [0u8; 12];
        nonce.copy_from_slice(&raw[8..20]);
        let mut payload = raw[20..].to_vec();
        let key = derive_chacha_key(SAVE_BUNDLE_KEY);
        chacha20_xor(&mut payload, &key, &nonce);
        match String::from_utf8(payload) {
            Ok(s) => s,
            Err(_) => return BTreeMap::new(),
        }
    } else {
        match String::from_utf8(raw) {
            Ok(s) => s,
            Err(_) => return BTreeMap::new(),
        }
    };
    match json_parse(&text) {
        Some(JsonValue::Object(m)) => m,
        _ => BTreeMap::new(),
    }
}

pub fn write_save_bundle(dir: &Path, root: &BTreeMap<String, JsonValue>) -> bool {
    if fs::create_dir_all(dir).is_err() {
        return false;
    }
    let mut json_out = String::new();
    let mut clone_root = BTreeMap::new();
    for (k, v) in root {
        let mut s = String::new();
        json_stringify(v, &mut s);
        if let Some(parsed) = json_parse(&s) {
            clone_root.insert(k.clone(), parsed);
        }
    }
    json_stringify(&JsonValue::Object(clone_root), &mut json_out);
    let mut payload = json_out.into_bytes();

    let now_ns = SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .map(|d| d.as_nanos() as u64)
        .unwrap_or(0x9876543210FEDCBA);
    let h = fnv1a_update(now_ns, &payload);
    let mut nonce = [0u8; 12];
    nonce[0..8].copy_from_slice(&h.to_le_bytes());
    nonce[8..12].copy_from_slice(&(now_ns as u32).to_le_bytes());

    let key = derive_chacha_key(SAVE_BUNDLE_KEY);
    chacha20_xor(&mut payload, &key, &nonce);

    let mut out = Vec::with_capacity(20 + payload.len());
    out.extend_from_slice(SAVE_BUNDLE_MAGIC);
    out.extend_from_slice(&nonce);
    out.extend_from_slice(&payload);

    let final_path = dir.join("savedata.qvn");
    let tmp_path = dir.join("savedata.qvn.tmp");
    if fs::write(&tmp_path, &out).is_ok() {
        if fs::rename(&tmp_path, &final_path).is_ok() {
            return true;
        }
        let _ = fs::remove_file(&tmp_path);
    }
    fs::write(&final_path, &out).is_ok()
}

fn sanitize_slot_name(slot: &str) -> String {
    let clean: String = slot
        .chars()
        .filter(|c| c.is_ascii_alphanumeric() || *c == '_' || *c == '-')
        .collect();
    if clean.is_empty() {
        String::from("quicksave")
    } else {
        clean
    }
}

fn epoch_secs_now() -> u64 {
    SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .map(|d| d.as_secs())
        .unwrap_or(0)
}

fn format_timestamp_now(secs: u64) -> String {
    let days = (secs / 86400) as i64;
    let rem = secs % 86400;
    let h = rem / 3600;
    let m = (rem % 3600) / 60;

    let z = days + 719468;
    let era = if z >= 0 { z } else { z - 146096 } / 146097;
    let doe = (z - era * 146097) as u64;
    let yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    let y = yoe as i64 + era * 400;
    let doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    let mp = (5 * doy + 2) / 153;
    let d = doy - (153 * mp + 2) / 5 + 1;
    let mo = if mp < 10 { mp + 3 } else { mp - 9 };
    let yr = y + if mo <= 2 { 1 } else { 0 };

    format!("{:04}-{:02}-{:02} {:02}:{:02}", yr, mo, d, h, m)
}

#[no_mangle]
pub extern "C" fn qvn_file_fingerprint(path: *const c_char) -> u64 {
    if path.is_null() {
        return 0;
    }
    let path_str = match unsafe { CStr::from_ptr(path) }.to_str() {
        Ok(s) => s,
        Err(_) => return 0,
    };
    let data = match fs::read(path_str) {
        Ok(d) => d,
        Err(_) => return 0,
    };
    let mut hash: u64 = 0xCBF29CE484222325;
    if let Ok(meta) = fs::metadata(path_str) {
        if let Ok(modified) = meta.modified() {
            if let Ok(dur) = modified.duration_since(UNIX_EPOCH) {
                hash = fnv1a_update(hash, &dur.as_secs().to_le_bytes());
                hash = fnv1a_update(hash, &dur.subsec_nanos().to_le_bytes());
            }
        }
        hash = fnv1a_update(hash, &meta.len().to_le_bytes());
    }
    hash = fnv1a_update(hash, &data);
    if hash == 0 { 1 } else { hash }
}

#[no_mangle]
pub extern "C" fn qvn_save_state_ex(
    dir: *const c_char,
    slot: *const c_char,
    command_idx: u64,
    choices: *const i32,
    choice_count: usize,
    js_state_json: *const c_char,
    speaker: *const c_char,
    preview: *const c_char,
) -> c_int {
    if dir.is_null() || slot.is_null() {
        return 0;
    }
    let dir_str = match unsafe { CStr::from_ptr(dir) }.to_str() {
        Ok(s) => s,
        Err(_) => return 0,
    };
    let slot_str = match unsafe { CStr::from_ptr(slot) }.to_str() {
        Ok(s) => sanitize_slot_name(s),
        Err(_) => return 0,
    };
    let js_json_val = if js_state_json.is_null() {
        JsonValue::Object(BTreeMap::new())
    } else {
        match unsafe { CStr::from_ptr(js_state_json) }
            .to_str()
            .ok()
            .and_then(json_parse)
        {
            Some(v) => v,
            None => JsonValue::Object(BTreeMap::new()),
        }
    };
    let spk_str = if speaker.is_null() {
        String::new()
    } else {
        unsafe { CStr::from_ptr(speaker) }.to_string_lossy().into_owned()
    };
    let prv_str = if preview.is_null() {
        String::new()
    } else {
        unsafe { CStr::from_ptr(preview) }.to_string_lossy().into_owned()
    };

    let choice_vec: Vec<JsonValue> = if !choices.is_null() && choice_count > 0 {
        let s = unsafe { slice::from_raw_parts(choices, choice_count) };
        s.iter().map(|&c| JsonValue::Number(c as f64)).collect()
    } else {
        Vec::new()
    };

    let now_epoch = epoch_secs_now();
    let mut slot_obj = BTreeMap::new();
    slot_obj.insert(String::from("command_idx"), JsonValue::Number(command_idx as f64));
    slot_obj.insert(String::from("choice_history"), JsonValue::Array(choice_vec));
    slot_obj.insert(String::from("js_state"), js_json_val);
    slot_obj.insert(String::from("speaker"), JsonValue::Str(spk_str));
    slot_obj.insert(String::from("preview"), JsonValue::Str(prv_str));
    slot_obj.insert(String::from("timestamp"), JsonValue::Str(format_timestamp_now(now_epoch)));
    slot_obj.insert(String::from("saved_epoch"), JsonValue::Number(now_epoch as f64));

    let dir_path = PathBuf::from(dir_str);
    if is_single_file_saves() {
        let mut bundle = read_save_bundle(&dir_path);
        let mut slots_map = match bundle.remove("slots") {
            Some(JsonValue::Object(m)) => m,
            _ => BTreeMap::new(),
        };
        slots_map.insert(slot_str, JsonValue::Object(slot_obj));
        bundle.insert(String::from("slots"), JsonValue::Object(slots_map));
        return if write_save_bundle(&dir_path, &bundle) { 1 } else { 0 };
    }

    let mut output = String::new();
    json_stringify(&JsonValue::Object(slot_obj), &mut output);

    if fs::create_dir_all(&dir_path).is_err() {
        return 0;
    }
    let final_path = dir_path.join(format!("{}.json", slot_str));
    let tmp_path = dir_path.join(format!("{}.json.tmp", slot_str));

    if fs::write(&tmp_path, output.as_bytes()).is_ok() {
        if fs::rename(&tmp_path, &final_path).is_ok() {
            return 1;
        }
        let _ = fs::remove_file(&tmp_path);
    }
    if fs::write(&final_path, output.as_bytes()).is_ok() { 1 } else { 0 }
}

#[no_mangle]
pub extern "C" fn qvn_save_state_json(
    dir: *const c_char,
    slot: *const c_char,
    command_idx: u64,
    choices: *const i32,
    choice_count: usize,
    js_state_json: *const c_char,
) -> c_int {
    qvn_save_state_ex(dir, slot, command_idx, choices, choice_count, js_state_json, ptr::null(), ptr::null())
}

fn load_slot_map(dir_path: &Path, slot_clean: &str) -> Option<BTreeMap<String, JsonValue>> {
    if is_single_file_saves() {
        let mut bundle = read_save_bundle(dir_path);
        if let Some(JsonValue::Object(mut slots)) = bundle.remove("slots") {
            if let Some(JsonValue::Object(m)) = slots.remove(slot_clean) {
                return Some(m);
            }
        }
        return None;
    }
    let path = dir_path.join(format!("{}.json", slot_clean));
    let content = fs::read_to_string(path).ok()?;
    match json_parse(&content) {
        Some(JsonValue::Object(m)) => Some(m),
        _ => None,
    }
}

#[no_mangle]
pub extern "C" fn qvn_load_state_json(
    dir: *const c_char,
    slot: *const c_char,
    out_data: *mut QvnSaveData,
) -> c_int {
    if dir.is_null() || slot.is_null() || out_data.is_null() {
        return 0;
    }
    unsafe {
        (*out_data).command_idx = 0;
        (*out_data).choice_history = ptr::null_mut();
        (*out_data).choice_count = 0;
        (*out_data).js_state_json = ptr::null_mut();
    }

    let dir_str = match unsafe { CStr::from_ptr(dir) }.to_str() {
        Ok(s) => s,
        Err(_) => return 0,
    };
    let slot_str = match unsafe { CStr::from_ptr(slot) }.to_str() {
        Ok(s) => sanitize_slot_name(s),
        Err(_) => return 0,
    };

    let root = match load_slot_map(Path::new(dir_str), &slot_str) {
        Some(m) => m,
        None => return 0,
    };

    let cmd_idx = match root.get("command_idx") {
        Some(JsonValue::Number(n)) => (*n).max(0.0) as u64,
        _ => 0,
    };

    let mut choices: Vec<i32> = Vec::new();
    if let Some(JsonValue::Array(arr)) = root.get("choice_history") {
        for item in arr {
            if let JsonValue::Number(n) = item {
                choices.push(*n as i32);
            }
        }
    }

    let mut state_str = String::new();
    if let Some(st) = root.get("js_state") {
        json_stringify(st, &mut state_str);
    } else {
        state_str.push_str("{}");
    }

    let c_state = match CString::new(state_str) {
        Ok(s) => s.into_raw(),
        Err(_) => return 0,
    };

    let choice_count = choices.len();
    let choice_ptr = if choice_count > 0 {
        let boxed = choices.into_boxed_slice();
        Box::into_raw(boxed) as *mut i32
    } else {
        ptr::null_mut()
    };

    unsafe {
        (*out_data).command_idx = cmd_idx;
        (*out_data).choice_history = choice_ptr;
        (*out_data).choice_count = choice_count;
        (*out_data).js_state_json = c_state;
    }
    1
}

#[no_mangle]
pub extern "C" fn qvn_get_save_slot_meta(
    dir: *const c_char,
    slot: *const c_char,
    out_meta: *mut QvnSaveSlotMeta,
) -> c_int {
    if dir.is_null() || slot.is_null() || out_meta.is_null() {
        return 0;
    }
    unsafe {
        (*out_meta).slot = [0; 64];
        (*out_meta).speaker = [0; 64];
        (*out_meta).preview = [0; 128];
        (*out_meta).timestamp = [0; 32];
        (*out_meta).saved_epoch = 0;
        (*out_meta).exists = 0;
    }
    let dir_str = match unsafe { CStr::from_ptr(dir) }.to_str() {
        Ok(s) => s,
        Err(_) => return 0,
    };
    let slot_raw = match unsafe { CStr::from_ptr(slot) }.to_str() {
        Ok(s) => s,
        Err(_) => return 0,
    };
    let slot_clean = sanitize_slot_name(slot_raw);
    unsafe {
        copy_str_to_buf(&slot_clean, &mut (*out_meta).slot);
    }

    let root = match load_slot_map(Path::new(dir_str), &slot_clean) {
        Some(m) => m,
        None => return 1,
    };
    unsafe {
        (*out_meta).exists = 1;
        if let Some(v) = get_str(&root, "speaker") { copy_str_to_buf(v, &mut (*out_meta).speaker); }
        if let Some(v) = get_str(&root, "preview") { copy_str_to_buf(v, &mut (*out_meta).preview); }
        if let Some(v) = get_str(&root, "timestamp") { copy_str_to_buf(v, &mut (*out_meta).timestamp); }
        if let Some(v) = get_u64(&root, "saved_epoch") { (*out_meta).saved_epoch = v; }
    }
    1
}

#[no_mangle]
pub extern "C" fn qvn_find_latest_save_slot(
    dir: *const c_char,
    out_slot_buf: *mut c_char,
    out_slot_cap: usize,
) -> c_int {
    if dir.is_null() || out_slot_buf.is_null() || out_slot_cap == 0 {
        return 0;
    }
    let dir_str = match unsafe { CStr::from_ptr(dir) }.to_str() {
        Ok(s) => s,
        Err(_) => return 0,
    };
    let mut best_slot: Option<String> = None;
    let mut best_epoch: u64 = 0;

    if is_single_file_saves() {
        let bundle = read_save_bundle(Path::new(dir_str));
        if let Some(JsonValue::Object(slots)) = bundle.get("slots") {
            for (k, v) in slots {
                if let JsonValue::Object(m) = v {
                    if m.contains_key("command_idx") {
                        let epoch = get_u64(m, "saved_epoch").unwrap_or(1);
                        if best_slot.is_none() || epoch >= best_epoch {
                            best_epoch = epoch;
                            best_slot = Some(k.clone());
                        }
                    }
                }
            }
        }
    } else if let Ok(entries) = fs::read_dir(dir_str) {
        for entry in entries.flatten() {
            let path = entry.path();
            if !path.is_file() {
                continue;
            }
            let stem = match path.file_stem().and_then(|s| s.to_str()) {
                Some(s) => s,
                None => continue,
            };
            if stem == "settings" || stem == "profile" {
                continue;
            }
            if path.extension().and_then(|s| s.to_str()) != Some("json") {
                continue;
            }
            if let Ok(content) = fs::read_to_string(&path) {
                if let Some(JsonValue::Object(m)) = json_parse(&content) {
                    if m.contains_key("command_idx") {
                        let epoch = get_u64(&m, "saved_epoch").unwrap_or(1);
                        if best_slot.is_none() || epoch >= best_epoch {
                            best_epoch = epoch;
                            best_slot = Some(String::from(stem));
                        }
                    }
                }
            }
        }
    }

    if let Some(slot) = best_slot {
        let out_slice = unsafe { slice::from_raw_parts_mut(out_slot_buf, out_slot_cap) };
        copy_str_to_buf(&slot, out_slice);
        1
    } else {
        0
    }
}

#[no_mangle]
pub extern "C" fn qvn_free_save_data(data: *mut QvnSaveData) {
    if data.is_null() {
        return;
    }
    unsafe {
        if !(*data).choice_history.is_null() && (*data).choice_count > 0 {
            let slice_ptr = ptr::slice_from_raw_parts_mut((*data).choice_history, (*data).choice_count);
            let _ = Box::from_raw(slice_ptr);
            (*data).choice_history = ptr::null_mut();
            (*data).choice_count = 0;
        }
        if !(*data).js_state_json.is_null() {
            let _ = CString::from_raw((*data).js_state_json);
            (*data).js_state_json = ptr::null_mut();
        }
    }
}
