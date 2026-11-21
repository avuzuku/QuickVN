pub mod config;
pub mod json;
pub mod mods;
pub mod pak;
pub mod projects;
pub mod rpy;
pub mod saves;
pub mod types;

use std::ffi::{CStr, CString};
use std::os::raw::c_char;

pub const BUILTIN_FONT: &[u8] = include_bytes!("../assets/font.ttf");

#[no_mangle]
pub extern "C" fn qvn_builtin_font_data(out_len: *mut usize) -> *const u8 {
    if !out_len.is_null() {
        unsafe {
            *out_len = BUILTIN_FONT.len();
        }
    }
    BUILTIN_FONT.as_ptr()
}

#[no_mangle]
pub extern "C" fn qvn_utf8_char_count(utf8_str: *const c_char) -> usize {
    if utf8_str.is_null() {
        return 0;
    }
    match unsafe { CStr::from_ptr(utf8_str) }.to_str() {
        Ok(s) => s.chars().count(),
        Err(_) => 0,
    }
}

#[no_mangle]
pub extern "C" fn qvn_utf8_prefix_bytes(utf8_str: *const c_char, max_chars: usize) -> usize {
    if utf8_str.is_null() {
        return 0;
    }
    let s = match unsafe { CStr::from_ptr(utf8_str) }.to_str() {
        Ok(v) => v,
        Err(_) => return 0,
    };
    if max_chars == 0 {
        return 0;
    }
    match s.char_indices().nth(max_chars) {
        Some((idx, _)) => idx,
        None => s.len(),
    }
}

#[no_mangle]
pub extern "C" fn qvn_lerp(a: f32, b: f32, t: f32) -> f32 {
    let clamped = t.clamp(0.0, 1.0);
    a + (b - a) * clamped
}

#[no_mangle]
pub extern "C" fn qvn_ease_out_cubic(t: f32) -> f32 {
    let x = t.clamp(0.0, 1.0);
    let inv = 1.0 - x;
    1.0 - inv * inv * inv
}

#[no_mangle]
pub extern "C" fn qvn_ease_in_out_quad(t: f32) -> f32 {
    let x = t.clamp(0.0, 1.0);
    if x < 0.5 {
        2.0 * x * x
    } else {
        1.0 - (-2.0 * x + 2.0).powi(2) / 2.0
    }
}

#[no_mangle]
pub extern "C" fn qvn_free_cstr(str_ptr: *mut c_char) {
    if !str_ptr.is_null() {
        unsafe {
            let _ = CString::from_raw(str_ptr);
        }
    }
}
