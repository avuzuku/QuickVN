use crate::json::escape_json_str;
use std::collections::{BTreeMap, BTreeSet};
use std::ffi::{CStr, CString};
use std::os::raw::c_char;
use std::ptr;

#[derive(Clone)]
struct RpyLine {
    indent: usize,
    text: String,
}

#[derive(Clone)]
struct RpyCharDef {
    display_name: String,
    color: String,
    image_prefix: String,
    is_narrator: bool,
}

fn strip_rpy_comment(raw: &str) -> String {
    let mut out = String::new();
    let mut in_quote = false;
    let mut quote_char = '\0';
    let mut escaped = false;

    for ch in raw.chars() {
        if escaped {
            out.push(ch);
            escaped = false;
            continue;
        }
        if in_quote {
            out.push(ch);
            if ch == '\\' {
                escaped = true;
            } else if ch == quote_char {
                in_quote = false;
            }
            continue;
        }
        if ch == '"' || ch == '\'' {
            in_quote = true;
            quote_char = ch;
            out.push(ch);
        } else if ch == '#' {
            break;
        } else {
            out.push(ch);
        }
    }
    out
}

fn parse_quoted_string_at(s: &str) -> Option<(String, &str)> {
    let trimmed = s.trim_start();
    let first = trimmed.chars().next()?;
    if first != '"' && first != '\'' {
        return None;
    }
    let mut escaped = false;
    let mut val = String::new();
    let mut end_idx = None;

    for (idx, ch) in trimmed.char_indices().skip(1) {
        if escaped {
            match ch {
                'n' => val.push('\n'),
                't' => val.push('\t'),
                'r' => val.push('\r'),
                other => val.push(other),
            }
            escaped = false;
        } else if ch == '\\' {
            escaped = true;
        } else if ch == first {
            end_idx = Some(idx + ch.len_utf8());
            break;
        } else {
            val.push(ch);
        }
    }
    let end = end_idx?;
    Some((val, &trimmed[end..]))
}

fn convert_rpy_interpolation(text: &str) -> String {
    let mut out = String::from("`");
    let chars: Vec<char> = text.chars().collect();
    let mut i = 0;
    while i < chars.len() {
        let ch = chars[i];
        if ch == '`' {
            out.push_str("\\`");
            i += 1;
        } else if ch == '\\' {
            out.push_str("\\\\");
            i += 1;
        } else if ch == '$' {
            out.push_str("\\$");
            i += 1;
        } else if ch == '\n' {
            out.push_str("\\n");
            i += 1;
        } else if ch == '[' {
            let mut j = i + 1;
            let mut var_name = String::new();
            while j < chars.len() && chars[j] != ']' {
                var_name.push(chars[j]);
                j += 1;
            }
            if j < chars.len() && !var_name.trim().is_empty() {
                out.push_str("${");
                out.push_str(var_name.trim());
                out.push('}');
                i = j + 1;
            } else {
                out.push('[');
                i += 1;
            }
        } else {
            out.push(ch);
            i += 1;
        }
    }
    out.push('`');
    out
}

fn convert_python_expr_to_js(expr: &str) -> String {
    let mut out = String::new();
    let bytes = expr.as_bytes();
    let mut i = 0;
    let mut in_quote = false;
    let mut qchar = b'"';
    let mut esc = false;

    while i < bytes.len() {
        let b = bytes[i];
        if esc {
            out.push(b as char);
            esc = false;
            i += 1;
            continue;
        }
        if in_quote {
            out.push(b as char);
            if b == b'\\' {
                esc = true;
            } else if b == qchar {
                in_quote = false;
            }
            i += 1;
            continue;
        }
        if b == b'"' || b == b'\'' {
            in_quote = true;
            qchar = b;
            out.push(b as char);
            i += 1;
            continue;
        }
        if b.is_ascii_alphabetic() || b == b'_' {
            let start = i;
            while i < bytes.len() && (bytes[i].is_ascii_alphanumeric() || bytes[i] == b'_') {
                i += 1;
            }
            let word = &expr[start..i];
            match word {
                "and" => out.push_str("&&"),
                "or" => out.push_str("||"),
                "not" => out.push('!'),
                "True" => out.push_str("true"),
                "False" => out.push_str("false"),
                "None" => out.push_str("null"),
                _ => out.push_str(word),
            }
        } else {
            out.push(b as char);
            i += 1;
        }
    }
    out
}

fn extract_rpy_kwarg(text: &str, key: &str) -> Option<String> {
    let pattern = format!("{}=", key);
    let idx = text.find(&pattern)?;
    let rest = text[idx + pattern.len()..].trim_start();
    if let Some((val, _)) = parse_quoted_string_at(rest) {
        return Some(val);
    }
    None
}

fn parse_rpy_define_char(line: &str) -> Option<(String, RpyCharDef)> {
    let rest = line.strip_prefix("define ")?.trim();
    let eq_idx = rest.find('=')?;
    let var_name = rest[..eq_idx].trim().to_string();
    let rhs = rest[eq_idx + 1..].trim();
    if !rhs.starts_with("Character(") {
        return None;
    }
    let after_open = rhs.strip_prefix("Character(")?;
    let inner = after_open.strip_suffix(')').unwrap_or(after_open).trim();
    if inner.starts_with("None") || inner.starts_with("null") {
        return Some((
            var_name,
            RpyCharDef {
                display_name: String::new(),
                color: String::new(),
                image_prefix: String::new(),
                is_narrator: true,
            },
        ));
    }
    let (display_name, _) = parse_quoted_string_at(inner)?;
    let color = extract_rpy_kwarg(inner, "color").unwrap_or_default();
    let image_prefix = extract_rpy_kwarg(inner, "image").unwrap_or_default();
    Some((
        var_name,
        RpyCharDef {
            display_name,
            color,
            image_prefix,
            is_narrator: false,
        },
    ))
}

fn emit_transition_fx(trans: &str, out: &mut String) {
    let t = trans.trim();
    if t == "flash" || t == "white" {
        out.push_str("flash(\"#FFFFFF\", 0.35);\n");
    } else if t == "vpunch" || t == "hpunch" || t == "shake" {
        out.push_str("shake(12, 0.35);\n");
    }
}

fn emit_simple_call(fn_name: &str, raw_arg: &str, out: &mut String) {
    let mut esc = String::new();
    escape_json_str(raw_arg.trim().trim_matches('"'), &mut esc);
    out.push_str(&format!("{}({});\n", fn_name, esc));
}

fn emit_rpy_block(
    lines: &[RpyLine],
    start: usize,
    end: usize,
    chars: &BTreeMap<String, RpyCharDef>,
    cont_counter: &mut usize,
    out: &mut String,
) -> bool {
    let mut i = start;
    let mut terminated = false;

    while i < end {
        let cur = &lines[i];
        let s = cur.text.as_str();

        if let Some(after) = s.strip_prefix("label ") {
            if let Some(lbl) = after.strip_suffix(':') {
                let lbl_name = lbl.trim();
                let mut body_end = i + 1;
                while body_end < end && lines[body_end].indent > cur.indent {
                    body_end += 1;
                }
                out.push_str(&format!("function __rpy_label_{}() {{\n", lbl_name));
                emit_rpy_block(lines, i + 1, body_end, chars, cont_counter, out);
                out.push_str("}\n");
                i = body_end;
                continue;
            }
        }

        if let Some(after) = s.strip_prefix("jump ") {
            out.push_str(&format!("__rpy_label_{}();\nreturn;\n", after.trim()));
            terminated = true;
            i += 1;
            continue;
        }

        if let Some(after) = s.strip_prefix("call ") {
            out.push_str(&format!("__rpy_label_{}();\n", after.trim()));
            i += 1;
            continue;
        }

        if s == "return" || s.starts_with("return ") {
            out.push_str("return;\n");
            terminated = true;
            i += 1;
            continue;
        }

        if s == "nvl on" || s == "nvl show" {
            out.push_str("nvl(true);\n");
            i += 1;
            continue;
        }
        if s == "nvl off" || s == "nvl hide" {
            out.push_str("nvl(false);\n");
            i += 1;
            continue;
        }
        if s == "nvl clear" {
            out.push_str("nvlClear();\n");
            i += 1;
            continue;
        }
        if s == "cinema on" {
            out.push_str("cinema(true);\n");
            i += 1;
            continue;
        }
        if s == "cinema off" {
            out.push_str("cinema(false);\n");
            i += 1;
            continue;
        }
        if s == "autosave" {
            out.push_str("autosave();\n");
            i += 1;
            continue;
        }
        if let Some(after) = s.strip_prefix("weather ") {
            emit_simple_call("weather", after, out);
            i += 1;
            continue;
        }
        if let Some(after) = s.strip_prefix("notify ") {
            emit_simple_call("notify", after, out);
            i += 1;
            continue;
        }
        if let Some(after) = s.strip_prefix("bounce ") {
            emit_simple_call("bounce", after, out);
            i += 1;
            continue;
        }
        if let Some(after) = s.strip_prefix("flip ") {
            emit_simple_call("flip", after, out);
            i += 1;
            continue;
        }
        if let Some(after) = s.strip_prefix("achievement ") {
            emit_simple_call("achievement", after, out);
            i += 1;
            continue;
        }
        if let Some(after) = s.strip_prefix("unlock gallery ") {
            emit_simple_call("unlockGallery", after, out);
            i += 1;
            continue;
        }
        if let Some(after) = s.strip_prefix("chapter ") {
            if let Some((t1, rest)) = parse_quoted_string_at(after) {
                let mut e1 = String::new();
                escape_json_str(&t1, &mut e1);
                if let Some((t2, _)) = parse_quoted_string_at(rest.trim()) {
                    let mut e2 = String::new();
                    escape_json_str(&t2, &mut e2);
                    out.push_str(&format!("chapter({}, {});\n", e1, e2));
                } else {
                    out.push_str(&format!("chapter({});\n", e1));
                }
            }
            i += 1;
            continue;
        }
        if let Some(after) = s.strip_prefix("ending ") {
            if let Some((t1, rest)) = parse_quoted_string_at(after) {
                let mut e1 = String::new();
                escape_json_str(&t1, &mut e1);
                if let Some((t2, _)) = parse_quoted_string_at(rest.trim()) {
                    let mut e2 = String::new();
                    escape_json_str(&t2, &mut e2);
                    out.push_str(&format!("ending({}, {});\n", e1, e2));
                } else {
                    out.push_str(&format!("ending({});\n", e1));
                }
            }
            terminated = true;
            i += 1;
            continue;
        }

        if s == "menu:" {
            let mut menu_end = i + 1;
            while menu_end < end && lines[menu_end].indent > cur.indent {
                menu_end += 1;
            }
            let has_continuation = menu_end < end;
            let cont_id = *cont_counter;
            *cont_counter += 1;
            if has_continuation {
                out.push_str(&format!("const __rpy_cont_{} = () => {{\n", cont_id));
                emit_rpy_block(lines, menu_end, end, chars, cont_counter, out);
                out.push_str("};\n");
            }

            out.push_str("choice({\n");
            let mut j = i + 1;
            let mut first_arm = true;
            while j < menu_end {
                let arm_line = &lines[j];
                if arm_line.text.ends_with(':') && (arm_line.text.starts_with('"') || arm_line.text.starts_with('\'')) {
                    if let Some((label_str, _)) = parse_quoted_string_at(&arm_line.text) {
                        let mut arm_end = j + 1;
                        while arm_end < menu_end && lines[arm_end].indent > arm_line.indent {
                            arm_end += 1;
                        }
                        if !first_arm {
                            out.push_str(",\n");
                        }
                        first_arm = false;
                        let mut escaped_lbl = String::new();
                        escape_json_str(&label_str, &mut escaped_lbl);
                        out.push_str(&format!("{}: () => {{\n", escaped_lbl));
                        let arm_terminated = emit_rpy_block(lines, j + 1, arm_end, chars, cont_counter, out);
                        if has_continuation && !arm_terminated {
                            out.push_str(&format!("__rpy_cont_{}();\n", cont_id));
                        }
                        out.push('}');
                        j = arm_end;
                        continue;
                    }
                }
                j += 1;
            }
            out.push_str("\n});\n");
            return true;
        }

        if s.starts_with("if ") && s.ends_with(':') {
            let mut cursor = i;
            let mut is_first_if = true;
            while cursor < end && lines[cursor].indent == cur.indent {
                let cond_line = &lines[cursor].text;
                let header = if is_first_if && cond_line.starts_with("if ") && cond_line.ends_with(':') {
                    is_first_if = false;
                    let cond = convert_python_expr_to_js(cond_line[3..cond_line.len() - 1].trim());
                    format!("if ({}) {{\n", cond)
                } else if cond_line.starts_with("elif ") && cond_line.ends_with(':') {
                    let cond = convert_python_expr_to_js(cond_line[5..cond_line.len() - 1].trim());
                    format!("else if ({}) {{\n", cond)
                } else if cond_line == "else:" {
                    String::from("else {\n")
                } else {
                    break;
                };

                let mut branch_end = cursor + 1;
                while branch_end < end && lines[branch_end].indent > cur.indent {
                    branch_end += 1;
                }
                out.push_str(&header);
                emit_rpy_block(lines, cursor + 1, branch_end, chars, cont_counter, out);
                out.push_str("}\n");
                cursor = branch_end;
                if cursor < end && lines[cursor].indent == cur.indent {
                    let nxt = &lines[cursor].text;
                    if !nxt.starts_with("elif ") && nxt != "else:" {
                        break;
                    }
                } else {
                    break;
                }
            }
            i = cursor;
            continue;
        }

        if let Some(after) = s.strip_prefix("scene ") {
            let (main_part, trans) = match after.split_once(" with ") {
                Some((a, b)) => (a.trim(), Some(b.trim())),
                None => (after.trim(), None),
            };
            let asset = if let Some((q, _)) = parse_quoted_string_at(main_part) {
                q
            } else {
                main_part.replace(' ', "_")
            };
            let mut esc = String::new();
            escape_json_str(&asset, &mut esc);
            out.push_str(&format!("bg({});\n", esc));
            if let Some(tr) = trans {
                emit_transition_fx(tr, out);
            }
            i += 1;
            continue;
        }

        if let Some(after) = s.strip_prefix("show ") {
            let (no_trans, trans) = match after.split_once(" with ") {
                Some((a, b)) => (a.trim(), Some(b.trim())),
                None => (after.trim(), None),
            };
            let (head, pos) = match no_trans.split_once(" at ") {
                Some((a, b)) => (a.trim(), b.trim()),
                None => (no_trans.trim(), ""),
            };
            let tokens: Vec<&str> = head.split_whitespace().collect();
            if !tokens.is_empty() {
                let id = tokens[0];
                let sprite = if tokens.len() > 1 {
                    let prefix = chars
                        .get(id)
                        .map(|c| c.image_prefix.as_str())
                        .filter(|p| !p.is_empty())
                        .unwrap_or("");
                    if !prefix.is_empty() {
                        format!("{}{}", prefix, tokens[1..].join("_"))
                    } else {
                        format!("{}_{}", id, tokens[1..].join("_"))
                    }
                } else {
                    String::from(id)
                };
                let mut esc_id = String::new();
                let mut esc_spr = String::new();
                let mut esc_pos = String::new();
                escape_json_str(id, &mut esc_id);
                escape_json_str(&sprite, &mut esc_spr);
                escape_json_str(pos, &mut esc_pos);
                if pos.is_empty() {
                    out.push_str(&format!("show({}, {});\n", esc_id, esc_spr));
                } else {
                    out.push_str(&format!("show({}, {}, {});\n", esc_id, esc_spr, esc_pos));
                }
            }
            if let Some(tr) = trans {
                emit_transition_fx(tr, out);
            }
            i += 1;
            continue;
        }

        if let Some(after) = s.strip_prefix("hide ") {
            let target = after.split(" with ").next().unwrap_or(after).trim();
            let id = target.split_whitespace().next().unwrap_or(target);
            if id == "all" {
                out.push_str("hideAll();\n");
            } else {
                let mut esc_id = String::new();
                escape_json_str(id, &mut esc_id);
                out.push_str(&format!("hide({});\n", esc_id));
            }
            i += 1;
            continue;
        }

        if let Some(after) = s.strip_prefix("play music ") {
            let trimmed = after.trim();
            let looping = !trimmed.contains("noloop");
            let clean = trimmed.replace("noloop", "");
            let path = if let Some((q, _)) = parse_quoted_string_at(clean.trim()) {
                q
            } else {
                clean.trim().to_string()
            };
            let mut esc = String::new();
            escape_json_str(&path, &mut esc);
            out.push_str(&format!("music({}, {});\n", esc, if looping { "true" } else { "false" }));
            i += 1;
            continue;
        }

        if s == "stop music" || s.starts_with("stop music ") {
            out.push_str("stopMusic();\n");
            i += 1;
            continue;
        }

        if let Some(after) = s.strip_prefix("play sound ") {
            let trimmed = after.trim();
            let path = if let Some((q, _)) = parse_quoted_string_at(trimmed) {
                q
            } else {
                trimmed.to_string()
            };
            let mut esc = String::new();
            escape_json_str(&path, &mut esc);
            out.push_str(&format!("sfx({});\n", esc));
            i += 1;
            continue;
        }

        if let Some(after) = s.strip_prefix("voice ") {
            let trimmed = after.trim();
            let path = if let Some((q, _)) = parse_quoted_string_at(trimmed) {
                q
            } else {
                trimmed.to_string()
            };
            let mut esc = String::new();
            escape_json_str(&path, &mut esc);
            out.push_str(&format!("voice({});\n", esc));
            i += 1;
            continue;
        }

        if let Some(after) = s.strip_prefix("pause ") {
            let secs = after.trim().parse::<f32>().unwrap_or(0.5);
            out.push_str(&format!("wait({});\n", secs));
            i += 1;
            continue;
        }

        if let Some(after) = s.strip_prefix("with ") {
            emit_transition_fx(after, out);
            i += 1;
            continue;
        }

        if let Some(after) = s.strip_prefix("$ ") {
            let js_stmt = convert_python_expr_to_js(after.trim());
            out.push_str(&js_stmt);
            if !js_stmt.ends_with(';') {
                out.push(';');
            }
            out.push('\n');
            i += 1;
            continue;
        }

        if s.starts_with('"') || s.starts_with('\'') {
            if let Some((first_str, rest)) = parse_quoted_string_at(s) {
                let rest_trim = rest.trim();
                if let Some((second_str, _)) = parse_quoted_string_at(rest_trim) {
                    let mut esc_who = String::new();
                    escape_json_str(&first_str, &mut esc_who);
                    let tpl = convert_rpy_interpolation(&second_str);
                    out.push_str(&format!("say({}, {});\n", esc_who, tpl));
                } else {
                    let tpl = convert_rpy_interpolation(&first_str);
                    out.push_str(&format!("n({});\n", tpl));
                }
                i += 1;
                continue;
            }
        }

        if let Some(sp_idx) = s.find(|c: char| c.is_whitespace()) {
            let head = &s[..sp_idx];
            let tail = s[sp_idx..].trim();
            if let Some((text_str, _)) = parse_quoted_string_at(tail) {
                let tpl = convert_rpy_interpolation(&text_str);
                if chars.contains_key(head) {
                    out.push_str(&format!("{}({});\n", head, tpl));
                } else {
                    let mut esc_who = String::new();
                    escape_json_str(head, &mut esc_who);
                    out.push_str(&format!("say({}, {});\n", esc_who, tpl));
                }
                i += 1;
                continue;
            }
        }

        i += 1;
    }

    terminated
}

fn transpile_rpy_to_js(src: &str) -> String {
    let mut chars: BTreeMap<String, RpyCharDef> = BTreeMap::new();
    let mut defaults: Vec<(String, String)> = Vec::new();
    let mut labels: BTreeSet<String> = BTreeSet::new();
    let mut body_lines: Vec<RpyLine> = Vec::new();

    for raw_line in src.lines() {
        let no_comment = strip_rpy_comment(raw_line);
        let trimmed_end = no_comment.trim_end();
        if trimmed_end.trim().is_empty() {
            continue;
        }
        let indent = trimmed_end.len() - trimmed_end.trim_start().len();
        let text = trimmed_end.trim().to_string();

        if indent == 0 {
            if let Some((var_name, def)) = parse_rpy_define_char(&text) {
                chars.insert(var_name, def);
                continue;
            }
            if let Some(after_def) = text.strip_prefix("default ") {
                if let Some((lhs, rhs)) = after_def.split_once('=') {
                    let var_name = lhs.trim().to_string();
                    let val_expr = convert_python_expr_to_js(rhs.trim());
                    defaults.push((var_name, val_expr));
                    continue;
                }
            }
            if let Some(after_lbl) = text.strip_prefix("label ") {
                if let Some(lbl) = after_lbl.strip_suffix(':') {
                    labels.insert(lbl.trim().to_string());
                }
            }
        }
        body_lines.push(RpyLine { indent, text });
    }

    let mut out = String::new();
    for (var_name, val_expr) in &defaults {
        out.push_str(&format!(
            "if (engine.state.{0} === undefined) engine.state.{0} = {1};\n",
            var_name, val_expr
        ));
        out.push_str(&format!(
            "Object.defineProperty(globalThis, \"{0}\", {{ get() {{ return engine.state.{0}; }}, set(v) {{ engine.state.{0} = v; }}, configurable: true }});\n",
            var_name
        ));
    }

    for (var_name, def) in &chars {
        if def.is_narrator {
            out.push_str(&format!("const {} = (t, cps) => n(t, cps);\n", var_name));
        } else {
            let mut esc_name = String::new();
            let mut esc_id = String::new();
            let mut esc_col = String::new();
            let mut esc_img = String::new();
            escape_json_str(&def.display_name, &mut esc_name);
            escape_json_str(var_name, &mut esc_id);
            escape_json_str(&def.color, &mut esc_col);
            escape_json_str(&def.image_prefix, &mut esc_img);
            out.push_str(&format!(
                "const {} = char({}, {{ id: {}, color: {}, base: {} }});\n",
                var_name, esc_name, esc_id, esc_col, esc_img
            ));
        }
    }

    let mut cont_counter = 1usize;
    emit_rpy_block(&body_lines, 0, body_lines.len(), &chars, &mut cont_counter, &mut out);

    if labels.contains("start") {
        out.push_str("__rpy_label_start();\n");
    }
    out
}

#[no_mangle]
pub extern "C" fn qvn_transpile_rpy(rpy_source: *const c_char) -> *mut c_char {
    if rpy_source.is_null() {
        return ptr::null_mut();
    }
    let src = match unsafe { CStr::from_ptr(rpy_source) }.to_str() {
        Ok(s) => s,
        Err(_) => return ptr::null_mut(),
    };
    let js = transpile_rpy_to_js(src);
    match CString::new(js) {
        Ok(c) => c.into_raw(),
        Err(_) => ptr::null_mut(),
    }
}