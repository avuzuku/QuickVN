use crate::types::QvnColor;
use std::collections::BTreeMap;

pub enum JsonValue {
    Null,
    Bool(bool),
    Number(f64),
    Str(String),
    Array(Vec<JsonValue>),
    Object(BTreeMap<String, JsonValue>),
}

struct JsonParser<'a> {
    bytes: &'a [u8],
    pos: usize,
}

fn skip_ws(p: &mut JsonParser<'_>) {
    while p.pos < p.bytes.len() {
        match p.bytes[p.pos] {
            b' ' | b'\t' | b'\n' | b'\r' => p.pos += 1,
            _ => break,
        }
    }
}

fn parse_hex4(p: &mut JsonParser<'_>) -> Option<u16> {
    if p.pos + 4 > p.bytes.len() {
        return None;
    }
    let slice = std::str::from_utf8(&p.bytes[p.pos..p.pos + 4]).ok()?;
    p.pos += 4;
    u16::from_str_radix(slice, 16).ok()
}

fn parse_string(p: &mut JsonParser<'_>) -> Option<String> {
    if p.pos >= p.bytes.len() || p.bytes[p.pos] != b'"' {
        return None;
    }
    p.pos += 1;
    let mut out = String::new();
    while p.pos < p.bytes.len() {
        let b = p.bytes[p.pos];
        p.pos += 1;
        if b == b'"' {
            return Some(out);
        }
        if b == b'\\' {
            if p.pos >= p.bytes.len() {
                return None;
            }
            let esc = p.bytes[p.pos];
            p.pos += 1;
            match esc {
                b'"' => out.push('"'),
                b'\\' => out.push('\\'),
                b'/' => out.push('/'),
                b'b' => out.push('\u{0008}'),
                b'f' => out.push('\u{000C}'),
                b'n' => out.push('\n'),
                b'r' => out.push('\r'),
                b't' => out.push('\t'),
                b'u' => {
                    let code = parse_hex4(p)?;
                    if (0xD800..=0xDBFF).contains(&code) {
                        if p.pos + 2 <= p.bytes.len()
                            && p.bytes[p.pos] == b'\\'
                            && p.bytes[p.pos + 1] == b'u'
                        {
                            p.pos += 2;
                            let low = parse_hex4(p)?;
                            if (0xDC00..=0xDFFF).contains(&low) {
                                let cp = 0x10000
                                    + (((code as u32 - 0xD800) << 10) | (low as u32 - 0xDC00));
                                out.push(char::from_u32(cp)?);
                            } else {
                                return None;
                            }
                        } else {
                            return None;
                        }
                    } else {
                        out.push(char::from_u32(code as u32)?);
                    }
                }
                _ => return None,
            }
        } else if b < 0x80 {
            out.push(b as char);
        } else {
            let start = p.pos - 1;
            let end = (start + 4).min(p.bytes.len());
            let mut decoded = None;
            for len in 2..=(end - start) {
                if let Ok(s) = std::str::from_utf8(&p.bytes[start..start + len]) {
                    if let Some(ch) = s.chars().next() {
                        decoded = Some((ch, len));
                        break;
                    }
                }
            }
            let (ch, len) = decoded?;
            out.push(ch);
            p.pos = start + len;
        }
    }
    None
}

fn parse_number(p: &mut JsonParser<'_>) -> Option<f64> {
    let start = p.pos;
    if p.pos < p.bytes.len() && p.bytes[p.pos] == b'-' {
        p.pos += 1;
    }
    while p.pos < p.bytes.len() && p.bytes[p.pos].is_ascii_digit() {
        p.pos += 1;
    }
    if p.pos < p.bytes.len() && p.bytes[p.pos] == b'.' {
        p.pos += 1;
        while p.pos < p.bytes.len() && p.bytes[p.pos].is_ascii_digit() {
            p.pos += 1;
        }
    }
    if p.pos < p.bytes.len() && (p.bytes[p.pos] == b'e' || p.bytes[p.pos] == b'E') {
        p.pos += 1;
        if p.pos < p.bytes.len() && (p.bytes[p.pos] == b'+' || p.bytes[p.pos] == b'-') {
            p.pos += 1;
        }
        while p.pos < p.bytes.len() && p.bytes[p.pos].is_ascii_digit() {
            p.pos += 1;
        }
    }
    let slice = std::str::from_utf8(&p.bytes[start..p.pos]).ok()?;
    slice.parse::<f64>().ok()
}

fn parse_value(p: &mut JsonParser<'_>) -> Option<JsonValue> {
    skip_ws(p);
    if p.pos >= p.bytes.len() {
        return None;
    }
    match p.bytes[p.pos] {
        b'n' => {
            if p.bytes[p.pos..].starts_with(b"null") {
                p.pos += 4;
                Some(JsonValue::Null)
            } else {
                None
            }
        }
        b't' => {
            if p.bytes[p.pos..].starts_with(b"true") {
                p.pos += 4;
                Some(JsonValue::Bool(true))
            } else {
                None
            }
        }
        b'f' => {
            if p.bytes[p.pos..].starts_with(b"false") {
                p.pos += 5;
                Some(JsonValue::Bool(false))
            } else {
                None
            }
        }
        b'"' => Some(JsonValue::Str(parse_string(p)?)),
        b'[' => {
            p.pos += 1;
            let mut arr = Vec::new();
            skip_ws(p);
            if p.pos < p.bytes.len() && p.bytes[p.pos] == b']' {
                p.pos += 1;
                return Some(JsonValue::Array(arr));
            }
            loop {
                arr.push(parse_value(p)?);
                skip_ws(p);
                if p.pos >= p.bytes.len() {
                    return None;
                }
                match p.bytes[p.pos] {
                    b',' => p.pos += 1,
                    b']' => {
                        p.pos += 1;
                        return Some(JsonValue::Array(arr));
                    }
                    _ => return None,
                }
            }
        }
        b'{' => {
            p.pos += 1;
            let mut map = BTreeMap::new();
            skip_ws(p);
            if p.pos < p.bytes.len() && p.bytes[p.pos] == b'}' {
                p.pos += 1;
                return Some(JsonValue::Object(map));
            }
            loop {
                skip_ws(p);
                let key = parse_string(p)?;
                skip_ws(p);
                if p.pos >= p.bytes.len() || p.bytes[p.pos] != b':' {
                    return None;
                }
                p.pos += 1;
                let val = parse_value(p)?;
                map.insert(key, val);
                skip_ws(p);
                if p.pos >= p.bytes.len() {
                    return None;
                }
                match p.bytes[p.pos] {
                    b',' => p.pos += 1,
                    b'}' => {
                        p.pos += 1;
                        return Some(JsonValue::Object(map));
                    }
                    _ => return None,
                }
            }
        }
        b'-' | b'0'..=b'9' => Some(JsonValue::Number(parse_number(p)?)),
        _ => None,
    }
}

pub fn json_parse(input: &str) -> Option<JsonValue> {
    let mut parser = JsonParser {
        bytes: input.as_bytes(),
        pos: 0,
    };
    parse_value(&mut parser)
}

pub fn escape_json_str(s: &str, out: &mut String) {
    out.push('"');
    for ch in s.chars() {
        match ch {
            '"' => out.push_str("\\\""),
            '\\' => out.push_str("\\\\"),
            '\n' => out.push_str("\\n"),
            '\r' => out.push_str("\\r"),
            '\t' => out.push_str("\\t"),
            c if (c as u32) < 0x20 => {
                out.push_str(&format!("\\u{:04x}", c as u32));
            }
            c => out.push(c),
        }
    }
    out.push('"');
}

pub fn json_stringify(val: &JsonValue, out: &mut String) {
    match val {
        JsonValue::Null => out.push_str("null"),
        JsonValue::Bool(b) => out.push_str(if *b { "true" } else { "false" }),
        JsonValue::Number(n) => {
            if n.is_finite() {
                out.push_str(&n.to_string());
            } else {
                out.push_str("null");
            }
        }
        JsonValue::Str(s) => escape_json_str(s, out),
        JsonValue::Array(arr) => {
            out.push('[');
            for (i, item) in arr.iter().enumerate() {
                if i > 0 {
                    out.push(',');
                }
                json_stringify(item, out);
            }
            out.push(']');
        }
        JsonValue::Object(map) => {
            out.push('{');
            for (i, (k, v)) in map.iter().enumerate() {
                if i > 0 {
                    out.push(',');
                }
                escape_json_str(k, out);
                out.push(':');
                json_stringify(v, out);
            }
            out.push('}');
        }
    }
}

pub fn get_obj<'a>(val: &'a JsonValue, key: &str) -> Option<&'a BTreeMap<String, JsonValue>> {
    if let JsonValue::Object(map) = val {
        if let Some(JsonValue::Object(sub)) = map.get(key) {
            return Some(sub);
        }
    }
    None
}

pub fn get_str<'a>(map: &'a BTreeMap<String, JsonValue>, key: &str) -> Option<&'a str> {
    if let Some(JsonValue::Str(s)) = map.get(key) {
        Some(s.as_str())
    } else {
        None
    }
}

pub fn get_f32(map: &BTreeMap<String, JsonValue>, key: &str) -> Option<f32> {
    if let Some(JsonValue::Number(n)) = map.get(key) {
        Some(*n as f32)
    } else {
        None
    }
}

pub fn get_u32(map: &BTreeMap<String, JsonValue>, key: &str) -> Option<u32> {
    if let Some(JsonValue::Number(n)) = map.get(key) {
        Some((*n).max(0.0) as u32)
    } else {
        None
    }
}

pub fn get_u64(map: &BTreeMap<String, JsonValue>, key: &str) -> Option<u64> {
    if let Some(JsonValue::Number(n)) = map.get(key) {
        Some((*n).max(0.0) as u64)
    } else {
        None
    }
}

pub fn get_u8(map: &BTreeMap<String, JsonValue>, key: &str) -> Option<u8> {
    if let Some(JsonValue::Number(n)) = map.get(key) {
        Some((*n).clamp(0.0, 255.0) as u8)
    } else {
        None
    }
}

pub fn get_bool(map: &BTreeMap<String, JsonValue>, key: &str) -> Option<u8> {
    if let Some(JsonValue::Bool(b)) = map.get(key) {
        Some(if *b { 1 } else { 0 })
    } else {
        None
    }
}

pub fn get_color(map: &BTreeMap<String, JsonValue>, key: &str) -> Option<QvnColor> {
    if let Some(JsonValue::Array(arr)) = map.get(key) {
        if arr.len() >= 3 {
            let r = match arr.first() {
                Some(JsonValue::Number(n)) => (*n).clamp(0.0, 255.0) as u8,
                _ => return None,
            };
            let g = match arr.get(1) {
                Some(JsonValue::Number(n)) => (*n).clamp(0.0, 255.0) as u8,
                _ => return None,
            };
            let b = match arr.get(2) {
                Some(JsonValue::Number(n)) => (*n).clamp(0.0, 255.0) as u8,
                _ => return None,
            };
            let a = match arr.get(3) {
                Some(JsonValue::Number(n)) => (*n).clamp(0.0, 255.0) as u8,
                _ => 255,
            };
            return Some(QvnColor { r, g, b, a });
        }
    }
    None
}