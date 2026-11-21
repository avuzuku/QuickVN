use crate::mods::scan_all_mods;
use crate::types::{buf_to_string, QvnBytes};
use std::collections::BTreeMap;
use std::ffi::CStr;
use std::fs;
use std::os::raw::{c_char, c_int};
use std::path::{Path, PathBuf};
use std::ptr;
use std::sync::{Mutex, OnceLock};
use std::time::{SystemTime, UNIX_EPOCH};

#[cfg(unix)]
use std::os::unix::fs::PermissionsExt;

const PAK_MAGIC_V1: &[u8; 8] = b"QVNPAK01";
const PAK_MAGIC_V2: &[u8; 8] = b"QVNPAK02";
const PAK_FOOTER_V1_SIZE: usize = 36;
const PAK_FOOTER_V2_SIZE: usize = 68;
const PAK_MASK_SEED: &str = "qvn-pak-v2-envelope-mask-2026";

struct VfsState {
    base_dir: PathBuf,
    game_mod_roots: Vec<PathBuf>,
    engine_mod_roots: Vec<PathBuf>,
    pak_files: BTreeMap<String, Vec<u8>>,
    has_pak: bool,
}

static VFS_LOCK: OnceLock<Mutex<VfsState>> = OnceLock::new();

fn vfs_mutex() -> &'static Mutex<VfsState> {
    VFS_LOCK.get_or_init(|| {
        Mutex::new(VfsState {
            base_dir: PathBuf::from("."),
            game_mod_roots: Vec::new(),
            engine_mod_roots: Vec::new(),
            pak_files: BTreeMap::new(),
            has_pak: false,
        })
    })
}

pub fn fnv1a_update(mut hash: u64, data: &[u8]) -> u64 {
    for &b in data {
        hash ^= b as u64;
        hash = hash.wrapping_mul(0x100000001B3);
    }
    hash
}

pub fn derive_chacha_key(key_str: &str) -> [u8; 32] {
    let bytes = key_str.as_bytes();
    let mut out = [0u8; 32];
    let mut state: u64 = 0xCBF29CE484222325;
    for round in 0..4u64 {
        state = fnv1a_update(state ^ round.wrapping_mul(0x9E3779B97F4A7C15), bytes);
        let mixed = state ^ (state >> 30);
        let mixed = mixed.wrapping_mul(0xBF58476D1CE4E5B9);
        let mixed = mixed ^ (mixed >> 27);
        let mixed = mixed.wrapping_mul(0x94D049BB133111EB);
        let mixed = mixed ^ (mixed >> 31);
        let chunk = mixed.to_le_bytes();
        let offset = (round as usize) * 8;
        out[offset..offset + 8].copy_from_slice(&chunk);
    }
    out
}

fn chacha_qr(s: &mut [u32; 16], a: usize, b: usize, c: usize, d: usize) {
    s[a] = s[a].wrapping_add(s[b]);
    s[d] ^= s[a];
    s[d] = s[d].rotate_left(16);

    s[c] = s[c].wrapping_add(s[d]);
    s[b] ^= s[c];
    s[b] = s[b].rotate_left(12);

    s[a] = s[a].wrapping_add(s[b]);
    s[d] ^= s[a];
    s[d] = s[d].rotate_left(8);

    s[c] = s[c].wrapping_add(s[d]);
    s[b] ^= s[c];
    s[b] = s[b].rotate_left(7);
}

fn chacha20_block(key: &[u8; 32], counter: u32, nonce: &[u8; 12]) -> [u8; 64] {
    let mut state = [0u32; 16];
    state[0] = 0x61707865;
    state[1] = 0x3320646E;
    state[2] = 0x79622D32;
    state[3] = 0x6B206574;
    for i in 0..8 {
        state[4 + i] = u32::from_le_bytes([
            key[i * 4],
            key[i * 4 + 1],
            key[i * 4 + 2],
            key[i * 4 + 3],
        ]);
    }
    state[12] = counter;
    for i in 0..3 {
        state[13 + i] = u32::from_le_bytes([
            nonce[i * 4],
            nonce[i * 4 + 1],
            nonce[i * 4 + 2],
            nonce[i * 4 + 3],
        ]);
    }

    let mut working = state;
    for _ in 0..10 {
        chacha_qr(&mut working, 0, 4, 8, 12);
        chacha_qr(&mut working, 1, 5, 9, 13);
        chacha_qr(&mut working, 2, 6, 10, 14);
        chacha_qr(&mut working, 3, 7, 11, 15);
        chacha_qr(&mut working, 0, 5, 10, 15);
        chacha_qr(&mut working, 1, 6, 11, 12);
        chacha_qr(&mut working, 2, 7, 8, 13);
        chacha_qr(&mut working, 3, 4, 9, 14);
    }

    let mut out = [0u8; 64];
    for i in 0..16 {
        let word = working[i].wrapping_add(state[i]).to_le_bytes();
        out[i * 4..i * 4 + 4].copy_from_slice(&word);
    }
    out
}

pub fn chacha20_xor(data: &mut [u8], key: &[u8; 32], nonce: &[u8; 12]) {
    let mut counter: u32 = 0;
    let mut offset = 0;
    while offset < data.len() {
        let block = chacha20_block(key, counter, nonce);
        let take = (data.len() - offset).min(64);
        for i in 0..take {
            data[offset + i] ^= block[i];
        }
        offset += take;
        counter = counter.wrapping_add(1);
    }
}

fn mask_pak_key(raw_key: &[u8; 32], nonce: &[u8; 12]) -> [u8; 32] {
    let env_key = derive_chacha_key(PAK_MASK_SEED);
    let block = chacha20_block(&env_key, 0x7A31_9C05, nonce);
    let mut out = *raw_key;
    for i in 0..32 {
        out[i] ^= block[i];
    }
    out
}

fn normalize_virtual_path(raw: &str) -> String {
    let replaced = raw.replace('\\', "/");
    let trimmed = replaced.trim_start_matches("./").trim_start_matches('/');
    String::from(trimmed)
}

fn read_u32_le(data: &[u8], pos: &mut usize) -> Option<u32> {
    if *pos + 4 > data.len() {
        return None;
    }
    let val = u32::from_le_bytes([data[*pos], data[*pos + 1], data[*pos + 2], data[*pos + 3]]);
    *pos += 4;
    Some(val)
}

fn read_u64_le(data: &[u8], pos: &mut usize) -> Option<u64> {
    if *pos + 8 > data.len() {
        return None;
    }
    let val = u64::from_le_bytes([
        data[*pos],
        data[*pos + 1],
        data[*pos + 2],
        data[*pos + 3],
        data[*pos + 4],
        data[*pos + 5],
        data[*pos + 6],
        data[*pos + 7],
    ]);
    *pos += 8;
    Some(val)
}

fn parse_pak_from_binary(bin_data: &[u8], key_str: &str) -> Option<BTreeMap<String, Vec<u8>>> {
    let (pak_offset, pak_size, nonce, key, footer_start) = if bin_data.len() >= PAK_FOOTER_V2_SIZE
        && &bin_data[bin_data.len() - 8..] == PAK_MAGIC_V2
    {
        let fs = bin_data.len() - PAK_FOOTER_V2_SIZE;
        let footer = &bin_data[fs..];
        let off = u64::from_le_bytes(footer[0..8].try_into().ok()?) as usize;
        let sz = u64::from_le_bytes(footer[8..16].try_into().ok()?) as usize;
        let mut nc = [0u8; 12];
        nc.copy_from_slice(&footer[16..28]);
        let mut masked = [0u8; 32];
        masked.copy_from_slice(&footer[28..60]);
        let unmasked = mask_pak_key(&masked, &nc);
        (off, sz, nc, unmasked, fs)
    } else if bin_data.len() >= PAK_FOOTER_V1_SIZE
        && &bin_data[bin_data.len() - 8..] == PAK_MAGIC_V1
    {
        let fs = bin_data.len() - PAK_FOOTER_V1_SIZE;
        let footer = &bin_data[fs..];
        let off = u64::from_le_bytes(footer[0..8].try_into().ok()?) as usize;
        let sz = u64::from_le_bytes(footer[8..16].try_into().ok()?) as usize;
        let mut nc = [0u8; 12];
        nc.copy_from_slice(&footer[16..28]);
        (off, sz, nc, derive_chacha_key(key_str), fs)
    } else {
        return None;
    };

    if pak_offset + pak_size != footer_start {
        return None;
    }

    let mut payload = bin_data[pak_offset..pak_offset + pak_size].to_vec();
    chacha20_xor(&mut payload, &key, &nonce);

    let mut pos = 0usize;
    let count = read_u32_le(&payload, &mut pos)? as usize;
    let mut map = BTreeMap::new();

    for _ in 0..count {
        let path_len = read_u32_le(&payload, &mut pos)? as usize;
        if pos + path_len > payload.len() {
            return None;
        }
        let path_str = std::str::from_utf8(&payload[pos..pos + path_len]).ok()?;
        pos += path_len;
        let data_len = read_u64_le(&payload, &mut pos)? as usize;
        if pos + data_len > payload.len() {
            return None;
        }
        let content = payload[pos..pos + data_len].to_vec();
        pos += data_len;
        map.insert(normalize_virtual_path(path_str), content);
    }

    Some(map)
}

fn collect_files_recursive(base: &Path, current: &Path, out: &mut Vec<(String, Vec<u8>)>) {
    let entries = match fs::read_dir(current) {
        Ok(e) => e,
        Err(_) => return,
    };
    let mut paths: Vec<PathBuf> = entries.flatten().map(|e| e.path()).collect();
    paths.sort();

    for path in paths {
        if path.is_dir() {
            collect_files_recursive(base, &path, out);
        } else if path.is_file() {
            if let Ok(rel) = path.strip_prefix(base) {
                if let Some(rel_str) = rel.to_str() {
                    if let Ok(data) = fs::read(&path) {
                        out.push((normalize_virtual_path(rel_str), data));
                    }
                }
            }
        }
    }
}

#[no_mangle]
pub extern "C" fn qvn_vfs_init(base_dir: *const c_char, pak_key: *const c_char) -> c_int {
    let base = if base_dir.is_null() {
        PathBuf::from(".")
    } else {
        match unsafe { CStr::from_ptr(base_dir) }.to_str() {
            Ok(s) if !s.is_empty() => PathBuf::from(s),
            _ => PathBuf::from("."),
        }
    };
    let key = if pak_key.is_null() {
        "quickvn-default-secret-key-2026"
    } else {
        match unsafe { CStr::from_ptr(pak_key) }.to_str() {
            Ok(s) => s,
            Err(_) => "quickvn-default-secret-key-2026",
        }
    };

    let mods = scan_all_mods(&base);
    let mut game_roots = Vec::new();
    let mut engine_roots = Vec::new();

    for m in mods {
        if m.enabled == 0 {
            continue;
        }
        let rp = PathBuf::from(buf_to_string(&m.root_path));
        if m.is_engine_mod != 0 {
            engine_roots.push(rp);
        } else {
            game_roots.push(rp);
        }
    }

    let mut pak_files = BTreeMap::new();
    let mut has_pak = false;
    if let Ok(exe_path) = std::env::current_exe() {
        if let Ok(bin) = fs::read(exe_path) {
            if let Some(map) = parse_pak_from_binary(&bin, key) {
                pak_files = map;
                has_pak = true;
            }
        }
    }

    if let Ok(mut guard) = vfs_mutex().lock() {
        guard.base_dir = base;
        guard.game_mod_roots = game_roots;
        guard.engine_mod_roots = engine_roots;
        guard.pak_files = pak_files;
        guard.has_pak = has_pak;
        return 1;
    }
    0
}

#[no_mangle]
pub extern "C" fn qvn_vfs_shutdown() {
    if let Ok(mut guard) = vfs_mutex().lock() {
        guard.game_mod_roots.clear();
        guard.engine_mod_roots.clear();
        guard.pak_files.clear();
        guard.has_pak = false;
    }
}

#[no_mangle]
pub extern "C" fn qvn_vfs_has_pak() -> c_int {
    if let Ok(guard) = vfs_mutex().lock() {
        if guard.has_pak { 1 } else { 0 }
    } else {
        0
    }
}

fn vfs_read_internal(virtual_path: *const c_char, out_bytes: *mut QvnBytes, include_mods: bool) -> c_int {
    if virtual_path.is_null() || out_bytes.is_null() {
        return 0;
    }
    unsafe {
        (*out_bytes).data = ptr::null_mut();
        (*out_bytes).len = 0;
    }
    let raw_path = match unsafe { CStr::from_ptr(virtual_path) }.to_str() {
        Ok(s) => s,
        Err(_) => return 0,
    };
    let norm = normalize_virtual_path(raw_path);

    let data_opt = if let Ok(guard) = vfs_mutex().lock() {
        let mut found: Option<Vec<u8>> = None;

        if include_mods {
            for root in guard.game_mod_roots.iter().rev() {
                let candidate = root.join(&norm);
                if candidate.is_file() {
                    if let Ok(d) = fs::read(candidate) {
                        found = Some(d);
                        break;
                    }
                }
            }
            if found.is_none() {
                for root in guard.engine_mod_roots.iter().rev() {
                    let candidate = root.join(&norm);
                    if candidate.is_file() {
                        if let Ok(d) = fs::read(candidate) {
                            found = Some(d);
                            break;
                        }
                    }
                }
            }
        }
        if found.is_none() {
            let disk_candidate = guard.base_dir.join(&norm);
            if disk_candidate.is_file() {
                if let Ok(d) = fs::read(disk_candidate) {
                    found = Some(d);
                }
            }
        }
        if found.is_none() {
            let direct = Path::new(raw_path);
            if direct.is_file() {
                if let Ok(d) = fs::read(direct) {
                    found = Some(d);
                }
            }
        }
        if found.is_none() {
            if let Some(mem) = guard.pak_files.get(&norm) {
                found = Some(mem.clone());
            }
        }
        found
    } else {
        None
    };

    if let Some(bytes) = data_opt {
        let len = bytes.len();
        let boxed = bytes.into_boxed_slice();
        unsafe {
            (*out_bytes).data = Box::into_raw(boxed) as *mut u8;
            (*out_bytes).len = len;
        }
        1
    } else {
        0
    }
}

#[no_mangle]
pub extern "C" fn qvn_vfs_read_file(virtual_path: *const c_char, out_bytes: *mut QvnBytes) -> c_int {
    vfs_read_internal(virtual_path, out_bytes, true)
}

#[no_mangle]
pub extern "C" fn qvn_vfs_read_base_file(virtual_path: *const c_char, out_bytes: *mut QvnBytes) -> c_int {
    vfs_read_internal(virtual_path, out_bytes, false)
}

#[no_mangle]
pub extern "C" fn qvn_bytes_free(bytes: *mut QvnBytes) {
    if bytes.is_null() {
        return;
    }
    unsafe {
        if !(*bytes).data.is_null() {
            let ptr_slice = ptr::slice_from_raw_parts_mut((*bytes).data, (*bytes).len);
            let _ = Box::from_raw(ptr_slice);
            (*bytes).data = ptr::null_mut();
            (*bytes).len = 0;
        }
    }
}

#[no_mangle]
pub extern "C" fn qvn_pack_executable(
    stub_exe: *const c_char,
    output_exe: *const c_char,
    project_dir: *const c_char,
    pak_key: *const c_char,
) -> c_int {
    if stub_exe.is_null() || output_exe.is_null() || project_dir.is_null() {
        return 0;
    }
    let stub_path = match unsafe { CStr::from_ptr(stub_exe) }.to_str() {
        Ok(s) => PathBuf::from(s),
        Err(_) => return 0,
    };
    let out_path = match unsafe { CStr::from_ptr(output_exe) }.to_str() {
        Ok(s) => PathBuf::from(s),
        Err(_) => return 0,
    };
    let proj_path = match unsafe { CStr::from_ptr(project_dir) }.to_str() {
        Ok(s) => PathBuf::from(s),
        Err(_) => return 0,
    };
    let key_str = if pak_key.is_null() {
        "quickvn-default-secret-key-2026"
    } else {
        match unsafe { CStr::from_ptr(pak_key) }.to_str() {
            Ok(s) => s,
            Err(_) => "quickvn-default-secret-key-2026",
        }
    };

    let mut stub_bytes = match fs::read(&stub_path) {
        Ok(b) => b,
        Err(_) => return 0,
    };

    if stub_bytes.len() >= PAK_FOOTER_V2_SIZE && &stub_bytes[stub_bytes.len() - 8..] == PAK_MAGIC_V2 {
        let fs = stub_bytes.len() - PAK_FOOTER_V2_SIZE;
        if let Ok(offset_bytes) = stub_bytes[fs..fs + 8].try_into() {
            let prev_offset = u64::from_le_bytes(offset_bytes) as usize;
            if prev_offset <= fs {
                stub_bytes.truncate(prev_offset);
            }
        }
    } else if stub_bytes.len() >= PAK_FOOTER_V1_SIZE && &stub_bytes[stub_bytes.len() - 8..] == PAK_MAGIC_V1 {
        let fs = stub_bytes.len() - PAK_FOOTER_V1_SIZE;
        if let Ok(offset_bytes) = stub_bytes[fs..fs + 8].try_into() {
            let prev_offset = u64::from_le_bytes(offset_bytes) as usize;
            if prev_offset <= fs {
                stub_bytes.truncate(prev_offset);
            }
        }
    }

    let mut entries: Vec<(String, Vec<u8>)> = Vec::new();
    let cfg_path = proj_path.join("config.json");
    if cfg_path.is_file() {
        if let Ok(d) = fs::read(&cfg_path) {
            entries.push((String::from("config.json"), d));
        }
    }
    collect_files_recursive(&proj_path, &proj_path.join("assets"), &mut entries);
    collect_files_recursive(&proj_path, &proj_path.join("scripts"), &mut entries);
    collect_files_recursive(&proj_path, &proj_path.join("locales"), &mut entries);

    let mut payload = Vec::new();
    payload.extend_from_slice(&(entries.len() as u32).to_le_bytes());
    for (rel_path, data) in &entries {
        let pbytes = rel_path.as_bytes();
        payload.extend_from_slice(&(pbytes.len() as u32).to_le_bytes());
        payload.extend_from_slice(pbytes);
        payload.extend_from_slice(&(data.len() as u64).to_le_bytes());
        payload.extend_from_slice(data);
    }

    let now_ns = SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .map(|d| d.as_nanos() as u64)
        .unwrap_or(0x123456789ABCDEF0);
    let content_hash = fnv1a_update(now_ns, &payload);
    let mut nonce = [0u8; 12];
    nonce[0..8].copy_from_slice(&content_hash.to_le_bytes());
    nonce[8..12].copy_from_slice(&(now_ns as u32).to_le_bytes());

    let key = derive_chacha_key(key_str);
    let masked_key = mask_pak_key(&key, &nonce);
    chacha20_xor(&mut payload, &key, &nonce);

    let pak_offset = stub_bytes.len() as u64;
    let pak_size = payload.len() as u64;

    stub_bytes.extend_from_slice(&payload);
    stub_bytes.extend_from_slice(&pak_offset.to_le_bytes());
    stub_bytes.extend_from_slice(&pak_size.to_le_bytes());
    stub_bytes.extend_from_slice(&nonce);
    stub_bytes.extend_from_slice(&masked_key);
    stub_bytes.extend_from_slice(PAK_MAGIC_V2);

    if let Some(parent) = out_path.parent() {
        if !parent.as_os_str().is_empty() {
            let _ = fs::create_dir_all(parent);
        }
    }

    if fs::write(&out_path, &stub_bytes).is_err() {
        return 0;
    }

    #[cfg(unix)]
    {
        if let Ok(meta) = fs::metadata(&out_path) {
            let mut perms = meta.permissions();
            perms.set_mode(0o755);
            let _ = fs::set_permissions(&out_path, perms);
        }
    }

    1
}
