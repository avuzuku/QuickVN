#include "script/qjs_bridge.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __has_include
    #if __has_include(<quickjs/quickjs.h>)
        #include <quickjs/quickjs.h>
    #else
        #include <quickjs.h>
    #endif
#else
    #include <quickjs.h>
#endif

typedef struct QjsCallbackSlot {
    int id;
    JSValue func;
} QjsCallbackSlot;

struct QjsBridge {
    JSRuntime* rt;
    JSContext* ctx;
    QjsCommandCallback callback;
    void* userdata;
    QjsCallbackSlot* slots;
    size_t slot_count;
    size_t slot_capacity;
    int next_callback_id;
};

static const char* QJS_DSL_PRELUDE =
    "Object.defineProperty(globalThis, 'state', { get: () => engine.state, set: (v) => { engine.state = v; }, configurable: true });\n"
    "engine.lang = 'en';\n"
    "function tr(val) {\n"
    "  if (val && typeof val === 'object' && !Array.isArray(val)) {\n"
    "    if (engine.lang in val) return String(val[engine.lang]);\n"
    "    if ('en' in val) return String(val.en);\n"
    "    const ks = Object.keys(val);\n"
    "    return ks.length ? String(val[ks[0]]) : '';\n"
    "  }\n"
    "  return val === undefined || val === null ? '' : String(val);\n"
    "}\n"
    "function bg(path, fade) { engine.showBackground(path, fade); }\n"
    "function show(idOrPath, pathOrPos, pos, fade, moveDur) {\n"
    "  if (arguments.length === 1) { engine.showCharacter(idOrPath); return; }\n"
    "  if (typeof pathOrPos === 'string' && (pathOrPos === 'left' || pathOrPos === 'center' || pathOrPos === 'right' || pathOrPos === 'far_left' || pathOrPos === 'far_right')) {\n"
    "    engine.showCharacter(idOrPath, pathOrPos, undefined, fade, moveDur, idOrPath);\n"
    "    return;\n"
    "  }\n"
    "  if (typeof pathOrPos === 'object' && pathOrPos !== null) {\n"
    "    engine.showCharacter(idOrPath, pathOrPos.x, pathOrPos.y, fade, moveDur, idOrPath);\n"
    "    return;\n"
    "  }\n"
    "  if (typeof pos === 'string') { engine.showCharacter(pathOrPos, pos, undefined, fade, moveDur, idOrPath); return; }\n"
    "  if (typeof pos === 'object' && pos !== null) { engine.showCharacter(pathOrPos, pos.x, pos.y, fade, moveDur, idOrPath); return; }\n"
    "  engine.showCharacter(pathOrPos, pos, undefined, fade, moveDur, idOrPath);\n"
    "}\n"
    "function move(id, pos, dur) {\n"
    "  if (typeof pos === 'string') { engine.moveCharacter(pos, undefined, dur, id); }\n"
    "  else if (typeof pos === 'object' && pos !== null) { engine.moveCharacter(pos.x, pos.y, dur, id); }\n"
    "  else { engine.moveCharacter(id, pos, dur); }\n"
    "}\n"
    "function hide(idOrFade, fade) {\n"
    "  if (typeof idOrFade === 'number' || idOrFade === undefined) { engine.hideCharacter(idOrFade); }\n"
    "  else { engine.hideCharacter(fade, idOrFade); }\n"
    "}\n"
    "function hideAll(fade) { engine.hideAllCharacters(fade); }\n"
    "function scaleChar(id, scale, dur) { engine.scaleCharacter(id, scale, dur); }\n"
    "function bounce(id) { engine.bounceCharacter(id); }\n"
    "function flip(id) { engine.flipCharacter(id); }\n"
    "function camZoom(scale, dur) { engine.camZoom(scale, dur); }\n"
    "function camPan(x, y, dur) { engine.camPan(x, y, dur); }\n"
    "function weather(type, density) { engine.weather(type, density); }\n"
    "function cinema(on) { engine.cinema(on === undefined ? true : !!on); }\n"
    "function vignette(alpha) { engine.vignette(alpha); }\n"
    "function say(who, text, cps, color, charId) { engine.say(tr(who), tr(text), cps, color, charId); }\n"
    "function n(text, cps) { engine.say('', tr(text), cps); }\n"
    "function thought(text, cps) { engine.nvl(true); engine.say('', tr(text), cps); }\n"
    "function nvl(on) { engine.nvl(on === undefined ? true : !!on); }\n"
    "function nvlClear() { engine.nvlClear(); }\n"
    "function music(path, loop, fade) { engine.playMusic(path, loop === undefined ? true : loop, fade); }\n"
    "function stopMusic(fade) { engine.stopMusic(fade); }\n"
    "function sfx(path) { engine.playSound(path); }\n"
    "function voice(path) { engine.playVoice(path); }\n"
    "function shake(intensity, duration) { engine.shake(intensity, duration); }\n"
    "function flash(colorHex, duration) { engine.flash(colorHex, duration); }\n"
    "function tint(colorHex, duration) { engine.tint(colorHex, duration); }\n"
    "function wait(seconds) { engine.wait(seconds); }\n"
    "function save(slot) { engine.save(slot); }\n"
    "function autosave() { engine.autosave(); }\n"
    "function load(slot) { engine.load(slot); }\n"
    "function unlockGallery(id) { engine.unlockGallery(id); }\n"
    "function achievement(id) { engine.unlockAchievement(id); }\n"
    "function notify(text) { engine.notify(tr(text)); }\n"
    "function chapter(title, subtitle, dur) { engine.chapter(tr(title), tr(subtitle), dur); }\n"
    "function input(promptText, stateKey, defVal) { engine.promptInput(tr(promptText), stateKey, tr(defVal)); }\n"
    "function ending(title, subtitle) { engine.ending(tr(title), tr(subtitle)); }\n"
    "function choice(opts, cb) {\n"
    "  if (Array.isArray(opts)) { engine.showChoices(opts.map(tr), cb); return; }\n"
    "  const labels = Object.keys(opts);\n"
    "  engine.showChoices(labels, (idx) => {\n"
    "    const fn = opts[labels[idx]];\n"
    "    if (typeof fn === 'function') fn(idx);\n"
    "  });\n"
    "}\n"
    "function timedChoice(seconds, defaultIdx, opts) {\n"
    "  const labels = Object.keys(opts);\n"
    "  engine.timedChoice(seconds, defaultIdx, labels, (idx) => {\n"
    "    const fn = opts[labels[idx]];\n"
    "    if (typeof fn === 'function') fn(idx);\n"
    "  });\n"
    "}\n"
    "function char(name, opts) {\n"
    "  opts = opts || {};\n"
    "  const id = opts.id || tr(name).toLowerCase();\n"
    "  const base = opts.base !== undefined ? opts.base : ((opts.dir || id) + '/');\n"
    "  const color = opts.color || '';\n"
    "  const fn = function(text, cps) { engine.say(tr(name), tr(text), cps, color, id); };\n"
    "  return new Proxy(fn, {\n"
    "    get(target, prop) {\n"
    "      if (prop in target) return target[prop];\n"
    "      if (typeof prop === 'string') {\n"
    "        return function(text, posOrCps, cpsArg) {\n"
    "          let pos = undefined;\n"
    "          let cps = undefined;\n"
    "          if (typeof posOrCps === 'string' || (typeof posOrCps === 'object' && posOrCps !== null)) {\n"
    "            pos = posOrCps;\n"
    "            cps = cpsArg;\n"
    "          } else {\n"
    "            cps = posOrCps;\n"
    "          }\n"
    "          if (base) { show(id, base + prop, pos); }\n"
    "          engine.say(tr(name), tr(text), cps, color, id);\n"
    "        };\n"
    "      }\n"
    "      return undefined;\n"
    "    }\n"
    "  });\n"
    "}\n";

static void qjs_print_exception(JSContext* ctx, const char* origin) {
    JSValue exception = JS_GetException(ctx);
    const char* err = JS_ToCString(ctx, exception);
    if (err) {
        fprintf(stderr, "QuickJS Error (%s): %s\n", origin, err);
        JS_FreeCString(ctx, err);
    }
    JS_FreeValue(ctx, exception);
}

static int qjs_store_callback(QjsBridge* bridge, JSValueConst func) {
    if (bridge->slot_count == bridge->slot_capacity) {
        size_t new_cap = bridge->slot_capacity == 0 ? 16 : bridge->slot_capacity * 2;
        QjsCallbackSlot* next = (QjsCallbackSlot*)realloc(bridge->slots, new_cap * sizeof(QjsCallbackSlot));
        if (!next) return -1;
        bridge->slots = next;
        bridge->slot_capacity = new_cap;
    }
    int id = bridge->next_callback_id++;
    bridge->slots[bridge->slot_count].id = id;
    bridge->slots[bridge->slot_count].func = JS_DupValue(bridge->ctx, func);
    bridge->slot_count++;
    return id;
}

static void qjs_clear_callbacks(QjsBridge* bridge) {
    if (!bridge) return;
    if (bridge->ctx && bridge->slots) {
        for (size_t i = 0; i < bridge->slot_count; ++i) {
            JS_FreeValue(bridge->ctx, bridge->slots[i].func);
        }
    }
    free(bridge->slots);
    bridge->slots = NULL;
    bridge->slot_count = 0;
    bridge->slot_capacity = 0;
    bridge->next_callback_id = 1;
}

static float qjs_opt_float(JSContext* ctx, int argc, JSValueConst* argv, int idx, float fallback) {
    if (idx >= argc || JS_IsUndefined(argv[idx]) || JS_IsNull(argv[idx])) return fallback;
    double val = 0.0;
    if (JS_ToFloat64(ctx, &val, argv[idx]) < 0) return fallback;
    return (float)val;
}

static const char* qjs_opt_cstr(JSContext* ctx, int argc, JSValueConst* argv, int idx) {
    if (idx >= argc || JS_IsUndefined(argv[idx]) || JS_IsNull(argv[idx])) return NULL;
    return JS_ToCString(ctx, argv[idx]);
}

static float qjs_preset_x(const char* preset) {
    if (!preset) return NAN;
    if (strcmp(preset, "far_left") == 0) return 180.0f;
    if (strcmp(preset, "left") == 0) return 340.0f;
    if (strcmp(preset, "center") == 0) return 640.0f;
    if (strcmp(preset, "right") == 0) return 940.0f;
    if (strcmp(preset, "far_right") == 0) return 1100.0f;
    return NAN;
}

static JSValue qjs_fn_say(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    (void)this_val;
    if (argc < 2) return JS_EXCEPTION;
    QjsBridge* bridge = (QjsBridge*)JS_GetContextOpaque(ctx);
    const char* name = JS_ToCString(ctx, argv[0]);
    const char* text = JS_ToCString(ctx, argv[1]);
    float cps = qjs_opt_float(ctx, argc, argv, 2, -1.0f);
    const char* color = qjs_opt_cstr(ctx, argc, argv, 3);
    const char* char_id = qjs_opt_cstr(ctx, argc, argv, 4);

    if (bridge && bridge->callback && name && text) {
        QjsCommandView cmd;
        memset(&cmd, 0, sizeof(cmd));
        cmd.type = QJS_CMD_SAY;
        cmd.arg1 = name;
        cmd.arg2 = text;
        cmd.arg3 = color ? color : "";
        cmd.arg4 = char_id ? char_id : "";
        cmd.num1 = cps;
        cmd.callback_id = -1;
        bridge->callback(&cmd, bridge->userdata);
    }

    JS_FreeCString(ctx, name);
    JS_FreeCString(ctx, text);
    if (color) JS_FreeCString(ctx, color);
    if (char_id) JS_FreeCString(ctx, char_id);
    return JS_UNDEFINED;
}

static JSValue qjs_fn_show_bg(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    (void)this_val;
    if (argc < 1) return JS_EXCEPTION;
    QjsBridge* bridge = (QjsBridge*)JS_GetContextOpaque(ctx);
    const char* path = JS_ToCString(ctx, argv[0]);
    float fade_dur = qjs_opt_float(ctx, argc, argv, 1, -1.0f);
    if (bridge && bridge->callback && path) {
        QjsCommandView cmd;
        memset(&cmd, 0, sizeof(cmd));
        cmd.type = QJS_CMD_SHOW_BACKGROUND;
        cmd.arg1 = path;
        cmd.duration1 = fade_dur;
        cmd.callback_id = -1;
        bridge->callback(&cmd, bridge->userdata);
    }
    JS_FreeCString(ctx, path);
    return JS_UNDEFINED;
}

static JSValue qjs_fn_show_char(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    (void)this_val;
    if (argc < 1) return JS_EXCEPTION;
    QjsBridge* bridge = (QjsBridge*)JS_GetContextOpaque(ctx);
    const char* path = JS_ToCString(ctx, argv[0]);
    float x = NAN;
    float y = NAN;
    if (argc >= 2 && JS_IsString(argv[1])) {
        const char* preset = JS_ToCString(ctx, argv[1]);
        x = qjs_preset_x(preset);
        if (preset) JS_FreeCString(ctx, preset);
    } else {
        x = qjs_opt_float(ctx, argc, argv, 1, NAN);
        y = qjs_opt_float(ctx, argc, argv, 2, NAN);
    }
    float fade_dur = qjs_opt_float(ctx, argc, argv, 3, -1.0f);
    float move_dur = qjs_opt_float(ctx, argc, argv, 4, -1.0f);
    const char* char_id = qjs_opt_cstr(ctx, argc, argv, 5);

    if (bridge && bridge->callback && path) {
        QjsCommandView cmd;
        memset(&cmd, 0, sizeof(cmd));
        cmd.type = QJS_CMD_SHOW_CHARACTER;
        cmd.arg1 = path;
        cmd.arg2 = char_id ? char_id : "default";
        cmd.num1 = x;
        cmd.num2 = y;
        cmd.duration1 = fade_dur;
        cmd.duration2 = move_dur;
        cmd.callback_id = -1;
        bridge->callback(&cmd, bridge->userdata);
    }
    JS_FreeCString(ctx, path);
    if (char_id) JS_FreeCString(ctx, char_id);
    return JS_UNDEFINED;
}

static JSValue qjs_fn_move_char(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    (void)this_val;
    if (argc < 1) return JS_EXCEPTION;
    QjsBridge* bridge = (QjsBridge*)JS_GetContextOpaque(ctx);
    float x = NAN;
    float y = NAN;
    if (JS_IsString(argv[0])) {
        const char* preset = JS_ToCString(ctx, argv[0]);
        x = qjs_preset_x(preset);
        if (preset) JS_FreeCString(ctx, preset);
    } else {
        x = qjs_opt_float(ctx, argc, argv, 0, NAN);
        y = qjs_opt_float(ctx, argc, argv, 1, NAN);
    }
    float move_dur = qjs_opt_float(ctx, argc, argv, 2, -1.0f);
    const char* char_id = qjs_opt_cstr(ctx, argc, argv, 3);

    if (bridge && bridge->callback) {
        QjsCommandView cmd;
        memset(&cmd, 0, sizeof(cmd));
        cmd.type = QJS_CMD_MOVE_CHARACTER;
        cmd.arg2 = char_id ? char_id : "default";
        cmd.num1 = x;
        cmd.num2 = y;
        cmd.duration1 = move_dur;
        cmd.callback_id = -1;
        bridge->callback(&cmd, bridge->userdata);
    }
    if (char_id) JS_FreeCString(ctx, char_id);
    return JS_UNDEFINED;
}

static JSValue qjs_fn_hide_char(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    (void)this_val;
    QjsBridge* bridge = (QjsBridge*)JS_GetContextOpaque(ctx);
    float fade_dur = qjs_opt_float(ctx, argc, argv, 0, -1.0f);
    const char* char_id = qjs_opt_cstr(ctx, argc, argv, 1);
    if (bridge && bridge->callback) {
        QjsCommandView cmd;
        memset(&cmd, 0, sizeof(cmd));
        cmd.type = QJS_CMD_HIDE_CHARACTER;
        cmd.arg2 = char_id ? char_id : "default";
        cmd.duration1 = fade_dur;
        cmd.callback_id = -1;
        bridge->callback(&cmd, bridge->userdata);
    }
    if (char_id) JS_FreeCString(ctx, char_id);
    return JS_UNDEFINED;
}

static JSValue qjs_fn_hide_all_chars(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    (void)this_val;
    QjsBridge* bridge = (QjsBridge*)JS_GetContextOpaque(ctx);
    float fade_dur = qjs_opt_float(ctx, argc, argv, 0, -1.0f);
    if (bridge && bridge->callback) {
        QjsCommandView cmd;
        memset(&cmd, 0, sizeof(cmd));
        cmd.type = QJS_CMD_HIDE_ALL_CHARACTERS;
        cmd.duration1 = fade_dur;
        cmd.callback_id = -1;
        bridge->callback(&cmd, bridge->userdata);
    }
    return JS_UNDEFINED;
}

static JSValue qjs_fn_scale_char(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    (void)this_val;
    if (argc < 2) return JS_EXCEPTION;
    QjsBridge* bridge = (QjsBridge*)JS_GetContextOpaque(ctx);
    const char* cid = JS_ToCString(ctx, argv[0]);
    float scale = qjs_opt_float(ctx, argc, argv, 1, 1.0f);
    float dur = qjs_opt_float(ctx, argc, argv, 2, 0.35f);
    if (bridge && bridge->callback && cid) {
        QjsCommandView cmd;
        memset(&cmd, 0, sizeof(cmd));
        cmd.type = QJS_CMD_SCALE_CHARACTER;
        cmd.arg2 = cid;
        cmd.num1 = scale;
        cmd.duration1 = dur;
        cmd.callback_id = -1;
        bridge->callback(&cmd, bridge->userdata);
    }
    JS_FreeCString(ctx, cid);
    return JS_UNDEFINED;
}

static JSValue qjs_fn_bounce_char(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    (void)this_val;
    QjsBridge* bridge = (QjsBridge*)JS_GetContextOpaque(ctx);
    const char* cid = qjs_opt_cstr(ctx, argc, argv, 0);
    if (bridge && bridge->callback) {
        QjsCommandView cmd;
        memset(&cmd, 0, sizeof(cmd));
        cmd.type = QJS_CMD_BOUNCE_CHARACTER;
        cmd.arg2 = cid ? cid : "default";
        cmd.callback_id = -1;
        bridge->callback(&cmd, bridge->userdata);
    }
    if (cid) JS_FreeCString(ctx, cid);
    return JS_UNDEFINED;
}

static JSValue qjs_fn_flip_char(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    (void)this_val;
    QjsBridge* bridge = (QjsBridge*)JS_GetContextOpaque(ctx);
    const char* cid = qjs_opt_cstr(ctx, argc, argv, 0);
    if (bridge && bridge->callback) {
        QjsCommandView cmd;
        memset(&cmd, 0, sizeof(cmd));
        cmd.type = QJS_CMD_FLIP_CHARACTER;
        cmd.arg2 = cid ? cid : "default";
        cmd.callback_id = -1;
        bridge->callback(&cmd, bridge->userdata);
    }
    if (cid) JS_FreeCString(ctx, cid);
    return JS_UNDEFINED;
}

static JSValue qjs_fn_cam_zoom(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    (void)this_val;
    QjsBridge* bridge = (QjsBridge*)JS_GetContextOpaque(ctx);
    float scale = qjs_opt_float(ctx, argc, argv, 0, 1.0f);
    float dur = qjs_opt_float(ctx, argc, argv, 1, 0.5f);
    if (bridge && bridge->callback) {
        QjsCommandView cmd;
        memset(&cmd, 0, sizeof(cmd));
        cmd.type = QJS_CMD_CAM_ZOOM;
        cmd.num1 = scale;
        cmd.duration1 = dur;
        cmd.callback_id = -1;
        bridge->callback(&cmd, bridge->userdata);
    }
    return JS_UNDEFINED;
}

static JSValue qjs_fn_cam_pan(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    (void)this_val;
    QjsBridge* bridge = (QjsBridge*)JS_GetContextOpaque(ctx);
    float x = qjs_opt_float(ctx, argc, argv, 0, 0.0f);
    float y = qjs_opt_float(ctx, argc, argv, 1, 0.0f);
    float dur = qjs_opt_float(ctx, argc, argv, 2, 0.5f);
    if (bridge && bridge->callback) {
        QjsCommandView cmd;
        memset(&cmd, 0, sizeof(cmd));
        cmd.type = QJS_CMD_CAM_PAN;
        cmd.num1 = x;
        cmd.num2 = y;
        cmd.duration1 = dur;
        cmd.callback_id = -1;
        bridge->callback(&cmd, bridge->userdata);
    }
    return JS_UNDEFINED;
}

static JSValue qjs_fn_weather(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    (void)this_val;
    if (argc < 1) return JS_EXCEPTION;
    QjsBridge* bridge = (QjsBridge*)JS_GetContextOpaque(ctx);
    const char* type = JS_ToCString(ctx, argv[0]);
    float density = qjs_opt_float(ctx, argc, argv, 1, 40.0f);
    if (bridge && bridge->callback && type) {
        QjsCommandView cmd;
        memset(&cmd, 0, sizeof(cmd));
        cmd.type = QJS_CMD_WEATHER;
        cmd.arg1 = type;
        cmd.num1 = density;
        cmd.callback_id = -1;
        bridge->callback(&cmd, bridge->userdata);
    }
    JS_FreeCString(ctx, type);
    return JS_UNDEFINED;
}

static JSValue qjs_fn_cinema(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    (void)this_val;
    QjsBridge* bridge = (QjsBridge*)JS_GetContextOpaque(ctx);
    int on = argc >= 1 ? (JS_ToBool(ctx, argv[0]) ? 1 : 0) : 1;
    if (bridge && bridge->callback) {
        QjsCommandView cmd;
        memset(&cmd, 0, sizeof(cmd));
        cmd.type = QJS_CMD_CINEMA;
        cmd.flag = on;
        cmd.callback_id = -1;
        bridge->callback(&cmd, bridge->userdata);
    }
    return JS_UNDEFINED;
}

static JSValue qjs_fn_vignette(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    (void)this_val;
    QjsBridge* bridge = (QjsBridge*)JS_GetContextOpaque(ctx);
    float alpha = qjs_opt_float(ctx, argc, argv, 0, 0.35f);
    if (bridge && bridge->callback) {
        QjsCommandView cmd;
        memset(&cmd, 0, sizeof(cmd));
        cmd.type = QJS_CMD_VIGNETTE;
        cmd.num1 = alpha;
        cmd.callback_id = -1;
        bridge->callback(&cmd, bridge->userdata);
    }
    return JS_UNDEFINED;
}

static JSValue qjs_fn_play_music(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    (void)this_val;
    if (argc < 1) return JS_EXCEPTION;
    QjsBridge* bridge = (QjsBridge*)JS_GetContextOpaque(ctx);
    const char* path = JS_ToCString(ctx, argv[0]);
    int loop = 1;
    if (argc >= 2 && !JS_IsUndefined(argv[1])) loop = JS_ToBool(ctx, argv[1]) ? 1 : 0;
    float fade = qjs_opt_float(ctx, argc, argv, 2, 0.4f);

    if (bridge && bridge->callback && path) {
        QjsCommandView cmd;
        memset(&cmd, 0, sizeof(cmd));
        cmd.type = QJS_CMD_PLAY_MUSIC;
        cmd.arg1 = path;
        cmd.flag = loop;
        cmd.duration1 = fade;
        cmd.callback_id = -1;
        bridge->callback(&cmd, bridge->userdata);
    }
    JS_FreeCString(ctx, path);
    return JS_UNDEFINED;
}

static JSValue qjs_fn_stop_music(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    (void)this_val;
    QjsBridge* bridge = (QjsBridge*)JS_GetContextOpaque(ctx);
    float fade = qjs_opt_float(ctx, argc, argv, 0, 0.4f);
    if (bridge && bridge->callback) {
        QjsCommandView cmd;
        memset(&cmd, 0, sizeof(cmd));
        cmd.type = QJS_CMD_STOP_MUSIC;
        cmd.duration1 = fade;
        cmd.callback_id = -1;
        bridge->callback(&cmd, bridge->userdata);
    }
    return JS_UNDEFINED;
}

static JSValue qjs_fn_play_sound(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    (void)this_val;
    if (argc < 1) return JS_EXCEPTION;
    QjsBridge* bridge = (QjsBridge*)JS_GetContextOpaque(ctx);
    const char* path = JS_ToCString(ctx, argv[0]);
    if (bridge && bridge->callback && path) {
        QjsCommandView cmd;
        memset(&cmd, 0, sizeof(cmd));
        cmd.type = QJS_CMD_PLAY_SOUND;
        cmd.arg1 = path;
        cmd.callback_id = -1;
        bridge->callback(&cmd, bridge->userdata);
    }
    JS_FreeCString(ctx, path);
    return JS_UNDEFINED;
}

static JSValue qjs_fn_play_voice(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    (void)this_val;
    if (argc < 1) return JS_EXCEPTION;
    QjsBridge* bridge = (QjsBridge*)JS_GetContextOpaque(ctx);
    const char* path = JS_ToCString(ctx, argv[0]);
    if (bridge && bridge->callback && path) {
        QjsCommandView cmd;
        memset(&cmd, 0, sizeof(cmd));
        cmd.type = QJS_CMD_PLAY_VOICE;
        cmd.arg1 = path;
        cmd.callback_id = -1;
        bridge->callback(&cmd, bridge->userdata);
    }
    JS_FreeCString(ctx, path);
    return JS_UNDEFINED;
}

static JSValue qjs_fn_show_choices(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    (void)this_val;
    if (argc < 2) return JS_EXCEPTION;
    if (!JS_IsArray(ctx, argv[0]) || !JS_IsFunction(ctx, argv[1])) return JS_EXCEPTION;

    QjsBridge* bridge = (QjsBridge*)JS_GetContextOpaque(ctx);
    JSValue len_val = JS_GetPropertyStr(ctx, argv[0], "length");
    uint32_t len = 0;
    JS_ToUint32(ctx, &len, len_val);
    JS_FreeValue(ctx, len_val);

    const char** items = len > 0 ? (const char**)calloc(len, sizeof(const char*)) : NULL;
    if (len > 0 && !items) return JS_EXCEPTION;

    size_t valid_count = 0;
    for (uint32_t i = 0; i < len; ++i) {
        JSValue item = JS_GetPropertyUint32(ctx, argv[0], i);
        const char* str = JS_ToCString(ctx, item);
        JS_FreeValue(ctx, item);
        if (str) items[valid_count++] = str;
    }

    if (bridge && bridge->callback) {
        int cb_id = qjs_store_callback(bridge, argv[1]);
        QjsCommandView cmd;
        memset(&cmd, 0, sizeof(cmd));
        cmd.type = QJS_CMD_SHOW_CHOICES;
        cmd.choices = items;
        cmd.choice_count = valid_count;
        cmd.callback_id = cb_id;
        bridge->callback(&cmd, bridge->userdata);
    }

    for (size_t i = 0; i < valid_count; ++i) JS_FreeCString(ctx, items[i]);
    free(items);
    return JS_UNDEFINED;
}

static JSValue qjs_fn_timed_choice(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    (void)this_val;
    if (argc < 4) return JS_EXCEPTION;
    if (!JS_IsArray(ctx, argv[2]) || !JS_IsFunction(ctx, argv[3])) return JS_EXCEPTION;

    QjsBridge* bridge = (QjsBridge*)JS_GetContextOpaque(ctx);
    float seconds = qjs_opt_float(ctx, argc, argv, 0, 10.0f);
    int32_t def_idx = 0;
    JS_ToInt32(ctx, &def_idx, argv[1]);

    JSValue len_val = JS_GetPropertyStr(ctx, argv[2], "length");
    uint32_t len = 0;
    JS_ToUint32(ctx, &len, len_val);
    JS_FreeValue(ctx, len_val);

    const char** items = len > 0 ? (const char**)calloc(len, sizeof(const char*)) : NULL;
    if (len > 0 && !items) return JS_EXCEPTION;

    size_t valid_count = 0;
    for (uint32_t i = 0; i < len; ++i) {
        JSValue item = JS_GetPropertyUint32(ctx, argv[2], i);
        const char* str = JS_ToCString(ctx, item);
        JS_FreeValue(ctx, item);
        if (str) items[valid_count++] = str;
    }

    if (bridge && bridge->callback) {
        int cb_id = qjs_store_callback(bridge, argv[3]);
        QjsCommandView cmd;
        memset(&cmd, 0, sizeof(cmd));
        cmd.type = QJS_CMD_TIMED_CHOICE;
        cmd.choices = items;
        cmd.choice_count = valid_count;
        cmd.callback_id = cb_id;
        cmd.duration1 = seconds;
        cmd.flag = def_idx;
        bridge->callback(&cmd, bridge->userdata);
    }

    for (size_t i = 0; i < valid_count; ++i) JS_FreeCString(ctx, items[i]);
    free(items);
    return JS_UNDEFINED;
}

static JSValue qjs_fn_prompt_input(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    (void)this_val;
    if (argc < 2) return JS_EXCEPTION;
    QjsBridge* bridge = (QjsBridge*)JS_GetContextOpaque(ctx);
    const char* prompt = JS_ToCString(ctx, argv[0]);
    const char* key = JS_ToCString(ctx, argv[1]);
    const char* def_val = qjs_opt_cstr(ctx, argc, argv, 2);
    if (bridge && bridge->callback && prompt && key) {
        QjsCommandView cmd;
        memset(&cmd, 0, sizeof(cmd));
        cmd.type = QJS_CMD_PROMPT_INPUT;
        cmd.arg1 = prompt;
        cmd.arg2 = key;
        cmd.arg3 = def_val ? def_val : "";
        cmd.callback_id = -1;
        bridge->callback(&cmd, bridge->userdata);
    }
    JS_FreeCString(ctx, prompt);
    JS_FreeCString(ctx, key);
    if (def_val) JS_FreeCString(ctx, def_val);
    return JS_UNDEFINED;
}

static JSValue qjs_fn_chapter(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    (void)this_val;
    if (argc < 1) return JS_EXCEPTION;
    QjsBridge* bridge = (QjsBridge*)JS_GetContextOpaque(ctx);
    const char* title = JS_ToCString(ctx, argv[0]);
    const char* sub = qjs_opt_cstr(ctx, argc, argv, 1);
    float dur = qjs_opt_float(ctx, argc, argv, 2, 2.5f);
    if (bridge && bridge->callback && title) {
        QjsCommandView cmd;
        memset(&cmd, 0, sizeof(cmd));
        cmd.type = QJS_CMD_CHAPTER;
        cmd.arg1 = title;
        cmd.arg2 = sub ? sub : "";
        cmd.duration1 = dur;
        cmd.callback_id = -1;
        bridge->callback(&cmd, bridge->userdata);
    }
    JS_FreeCString(ctx, title);
    if (sub) JS_FreeCString(ctx, sub);
    return JS_UNDEFINED;
}

static JSValue qjs_fn_notify(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    (void)this_val;
    if (argc < 1) return JS_EXCEPTION;
    QjsBridge* bridge = (QjsBridge*)JS_GetContextOpaque(ctx);
    const char* text = JS_ToCString(ctx, argv[0]);
    if (bridge && bridge->callback && text) {
        QjsCommandView cmd;
        memset(&cmd, 0, sizeof(cmd));
        cmd.type = QJS_CMD_NOTIFY;
        cmd.arg1 = text;
        cmd.callback_id = -1;
        bridge->callback(&cmd, bridge->userdata);
    }
    JS_FreeCString(ctx, text);
    return JS_UNDEFINED;
}

static JSValue qjs_fn_save(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    (void)this_val;
    QjsBridge* bridge = (QjsBridge*)JS_GetContextOpaque(ctx);
    const char* slot = qjs_opt_cstr(ctx, argc, argv, 0);
    if (bridge && bridge->callback) {
        QjsCommandView cmd;
        memset(&cmd, 0, sizeof(cmd));
        cmd.type = QJS_CMD_SAVE_STATE;
        cmd.arg1 = slot ? slot : "";
        cmd.callback_id = -1;
        bridge->callback(&cmd, bridge->userdata);
    }
    if (slot) JS_FreeCString(ctx, slot);
    return JS_UNDEFINED;
}

static JSValue qjs_fn_autosave(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    (void)this_val;
    (void)argc;
    (void)argv;
    QjsBridge* bridge = (QjsBridge*)JS_GetContextOpaque(ctx);
    if (bridge && bridge->callback) {
        QjsCommandView cmd;
        memset(&cmd, 0, sizeof(cmd));
        cmd.type = QJS_CMD_AUTOSAVE;
        cmd.arg1 = "autosave";
        cmd.callback_id = -1;
        bridge->callback(&cmd, bridge->userdata);
    }
    return JS_UNDEFINED;
}

static JSValue qjs_fn_load(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    (void)this_val;
    QjsBridge* bridge = (QjsBridge*)JS_GetContextOpaque(ctx);
    const char* slot = qjs_opt_cstr(ctx, argc, argv, 0);
    if (bridge && bridge->callback) {
        QjsCommandView cmd;
        memset(&cmd, 0, sizeof(cmd));
        cmd.type = QJS_CMD_LOAD_STATE;
        cmd.arg1 = slot ? slot : "";
        cmd.callback_id = -1;
        bridge->callback(&cmd, bridge->userdata);
    }
    if (slot) JS_FreeCString(ctx, slot);
    return JS_UNDEFINED;
}

static JSValue qjs_fn_shake(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    (void)this_val;
    QjsBridge* bridge = (QjsBridge*)JS_GetContextOpaque(ctx);
    float intensity = qjs_opt_float(ctx, argc, argv, 0, 12.0f);
    float duration = qjs_opt_float(ctx, argc, argv, 1, 0.35f);
    if (bridge && bridge->callback) {
        QjsCommandView cmd;
        memset(&cmd, 0, sizeof(cmd));
        cmd.type = QJS_CMD_SHAKE;
        cmd.num1 = intensity;
        cmd.duration1 = duration;
        cmd.callback_id = -1;
        bridge->callback(&cmd, bridge->userdata);
    }
    return JS_UNDEFINED;
}

static JSValue qjs_fn_flash(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    (void)this_val;
    QjsBridge* bridge = (QjsBridge*)JS_GetContextOpaque(ctx);
    const char* color_hex = qjs_opt_cstr(ctx, argc, argv, 0);
    float duration = qjs_opt_float(ctx, argc, argv, 1, 0.3f);
    if (bridge && bridge->callback) {
        QjsCommandView cmd;
        memset(&cmd, 0, sizeof(cmd));
        cmd.type = QJS_CMD_FLASH;
        cmd.arg1 = color_hex ? color_hex : "#FFFFFF";
        cmd.duration1 = duration;
        cmd.callback_id = -1;
        bridge->callback(&cmd, bridge->userdata);
    }
    if (color_hex) JS_FreeCString(ctx, color_hex);
    return JS_UNDEFINED;
}

static JSValue qjs_fn_tint(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    (void)this_val;
    QjsBridge* bridge = (QjsBridge*)JS_GetContextOpaque(ctx);
    const char* color_hex = qjs_opt_cstr(ctx, argc, argv, 0);
    float duration = qjs_opt_float(ctx, argc, argv, 1, 0.4f);
    if (bridge && bridge->callback) {
        QjsCommandView cmd;
        memset(&cmd, 0, sizeof(cmd));
        cmd.type = QJS_CMD_TINT;
        cmd.arg1 = color_hex ? color_hex : "#00000000";
        cmd.duration1 = duration;
        cmd.callback_id = -1;
        bridge->callback(&cmd, bridge->userdata);
    }
    if (color_hex) JS_FreeCString(ctx, color_hex);
    return JS_UNDEFINED;
}

static JSValue qjs_fn_wait(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    (void)this_val;
    QjsBridge* bridge = (QjsBridge*)JS_GetContextOpaque(ctx);
    float seconds = qjs_opt_float(ctx, argc, argv, 0, 0.5f);
    if (bridge && bridge->callback) {
        QjsCommandView cmd;
        memset(&cmd, 0, sizeof(cmd));
        cmd.type = QJS_CMD_WAIT;
        cmd.duration1 = seconds;
        cmd.callback_id = -1;
        bridge->callback(&cmd, bridge->userdata);
    }
    return JS_UNDEFINED;
}

static JSValue qjs_fn_nvl(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    (void)this_val;
    QjsBridge* bridge = (QjsBridge*)JS_GetContextOpaque(ctx);
    int enabled = argc >= 1 ? (JS_ToBool(ctx, argv[0]) ? 1 : 0) : 1;
    if (bridge && bridge->callback) {
        QjsCommandView cmd;
        memset(&cmd, 0, sizeof(cmd));
        cmd.type = QJS_CMD_NVL_MODE;
        cmd.flag = enabled;
        cmd.callback_id = -1;
        bridge->callback(&cmd, bridge->userdata);
    }
    return JS_UNDEFINED;
}

static JSValue qjs_fn_nvl_clear(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    (void)this_val;
    (void)argc;
    (void)argv;
    QjsBridge* bridge = (QjsBridge*)JS_GetContextOpaque(ctx);
    if (bridge && bridge->callback) {
        QjsCommandView cmd;
        memset(&cmd, 0, sizeof(cmd));
        cmd.type = QJS_CMD_NVL_CLEAR;
        cmd.callback_id = -1;
        bridge->callback(&cmd, bridge->userdata);
    }
    return JS_UNDEFINED;
}

static JSValue qjs_fn_unlock_gallery(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    (void)this_val;
    if (argc < 1) return JS_EXCEPTION;
    QjsBridge* bridge = (QjsBridge*)JS_GetContextOpaque(ctx);
    const char* id = JS_ToCString(ctx, argv[0]);
    if (bridge && bridge->callback && id) {
        QjsCommandView cmd;
        memset(&cmd, 0, sizeof(cmd));
        cmd.type = QJS_CMD_UNLOCK_GALLERY;
        cmd.arg1 = id;
        cmd.callback_id = -1;
        bridge->callback(&cmd, bridge->userdata);
    }
    JS_FreeCString(ctx, id);
    return JS_UNDEFINED;
}

static JSValue qjs_fn_unlock_achievement(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    (void)this_val;
    if (argc < 1) return JS_EXCEPTION;
    QjsBridge* bridge = (QjsBridge*)JS_GetContextOpaque(ctx);
    const char* id = JS_ToCString(ctx, argv[0]);
    if (bridge && bridge->callback && id) {
        QjsCommandView cmd;
        memset(&cmd, 0, sizeof(cmd));
        cmd.type = QJS_CMD_UNLOCK_ACHIEVEMENT;
        cmd.arg1 = id;
        cmd.callback_id = -1;
        bridge->callback(&cmd, bridge->userdata);
    }
    JS_FreeCString(ctx, id);
    return JS_UNDEFINED;
}

static JSValue qjs_fn_ending(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    (void)this_val;
    QjsBridge* bridge = (QjsBridge*)JS_GetContextOpaque(ctx);
    const char* title = qjs_opt_cstr(ctx, argc, argv, 0);
    const char* sub = qjs_opt_cstr(ctx, argc, argv, 1);
    if (bridge && bridge->callback) {
        QjsCommandView cmd;
        memset(&cmd, 0, sizeof(cmd));
        cmd.type = QJS_CMD_ENDING;
        cmd.arg1 = title ? title : "";
        cmd.arg2 = sub ? sub : "";
        cmd.callback_id = -1;
        bridge->callback(&cmd, bridge->userdata);
    }
    if (title) JS_FreeCString(ctx, title);
    if (sub) JS_FreeCString(ctx, sub);
    return JS_UNDEFINED;
}

static void qjs_register_bindings(QjsBridge* bridge) {
    JSValue global_obj = JS_GetGlobalObject(bridge->ctx);
    JSValue engine_obj = JS_NewObject(bridge->ctx);
    JSValue state_obj = JS_NewObject(bridge->ctx);

    JS_SetPropertyStr(bridge->ctx, engine_obj, "state", state_obj);
    JS_SetPropertyStr(bridge->ctx, engine_obj, "say", JS_NewCFunction(bridge->ctx, qjs_fn_say, "say", 5));
    JS_SetPropertyStr(bridge->ctx, engine_obj, "showBackground", JS_NewCFunction(bridge->ctx, qjs_fn_show_bg, "showBackground", 2));
    JS_SetPropertyStr(bridge->ctx, engine_obj, "showCharacter", JS_NewCFunction(bridge->ctx, qjs_fn_show_char, "showCharacter", 6));
    JS_SetPropertyStr(bridge->ctx, engine_obj, "moveCharacter", JS_NewCFunction(bridge->ctx, qjs_fn_move_char, "moveCharacter", 4));
    JS_SetPropertyStr(bridge->ctx, engine_obj, "hideCharacter", JS_NewCFunction(bridge->ctx, qjs_fn_hide_char, "hideCharacter", 2));
    JS_SetPropertyStr(bridge->ctx, engine_obj, "hideAllCharacters", JS_NewCFunction(bridge->ctx, qjs_fn_hide_all_chars, "hideAllCharacters", 1));
    JS_SetPropertyStr(bridge->ctx, engine_obj, "scaleCharacter", JS_NewCFunction(bridge->ctx, qjs_fn_scale_char, "scaleCharacter", 3));
    JS_SetPropertyStr(bridge->ctx, engine_obj, "bounceCharacter", JS_NewCFunction(bridge->ctx, qjs_fn_bounce_char, "bounceCharacter", 1));
    JS_SetPropertyStr(bridge->ctx, engine_obj, "flipCharacter", JS_NewCFunction(bridge->ctx, qjs_fn_flip_char, "flipCharacter", 1));
    JS_SetPropertyStr(bridge->ctx, engine_obj, "camZoom", JS_NewCFunction(bridge->ctx, qjs_fn_cam_zoom, "camZoom", 2));
    JS_SetPropertyStr(bridge->ctx, engine_obj, "camPan", JS_NewCFunction(bridge->ctx, qjs_fn_cam_pan, "camPan", 3));
    JS_SetPropertyStr(bridge->ctx, engine_obj, "weather", JS_NewCFunction(bridge->ctx, qjs_fn_weather, "weather", 2));
    JS_SetPropertyStr(bridge->ctx, engine_obj, "cinema", JS_NewCFunction(bridge->ctx, qjs_fn_cinema, "cinema", 1));
    JS_SetPropertyStr(bridge->ctx, engine_obj, "vignette", JS_NewCFunction(bridge->ctx, qjs_fn_vignette, "vignette", 1));
    JS_SetPropertyStr(bridge->ctx, engine_obj, "playMusic", JS_NewCFunction(bridge->ctx, qjs_fn_play_music, "playMusic", 3));
    JS_SetPropertyStr(bridge->ctx, engine_obj, "stopMusic", JS_NewCFunction(bridge->ctx, qjs_fn_stop_music, "stopMusic", 1));
    JS_SetPropertyStr(bridge->ctx, engine_obj, "playSound", JS_NewCFunction(bridge->ctx, qjs_fn_play_sound, "playSound", 1));
    JS_SetPropertyStr(bridge->ctx, engine_obj, "playVoice", JS_NewCFunction(bridge->ctx, qjs_fn_play_voice, "playVoice", 1));
    JS_SetPropertyStr(bridge->ctx, engine_obj, "showChoices", JS_NewCFunction(bridge->ctx, qjs_fn_show_choices, "showChoices", 2));
    JS_SetPropertyStr(bridge->ctx, engine_obj, "timedChoice", JS_NewCFunction(bridge->ctx, qjs_fn_timed_choice, "timedChoice", 4));
    JS_SetPropertyStr(bridge->ctx, engine_obj, "promptInput", JS_NewCFunction(bridge->ctx, qjs_fn_prompt_input, "promptInput", 3));
    JS_SetPropertyStr(bridge->ctx, engine_obj, "chapter", JS_NewCFunction(bridge->ctx, qjs_fn_chapter, "chapter", 3));
    JS_SetPropertyStr(bridge->ctx, engine_obj, "notify", JS_NewCFunction(bridge->ctx, qjs_fn_notify, "notify", 1));
    JS_SetPropertyStr(bridge->ctx, engine_obj, "save", JS_NewCFunction(bridge->ctx, qjs_fn_save, "save", 1));
    JS_SetPropertyStr(bridge->ctx, engine_obj, "autosave", JS_NewCFunction(bridge->ctx, qjs_fn_autosave, "autosave", 0));
    JS_SetPropertyStr(bridge->ctx, engine_obj, "load", JS_NewCFunction(bridge->ctx, qjs_fn_load, "load", 1));
    JS_SetPropertyStr(bridge->ctx, engine_obj, "shake", JS_NewCFunction(bridge->ctx, qjs_fn_shake, "shake", 2));
    JS_SetPropertyStr(bridge->ctx, engine_obj, "flash", JS_NewCFunction(bridge->ctx, qjs_fn_flash, "flash", 2));
    JS_SetPropertyStr(bridge->ctx, engine_obj, "tint", JS_NewCFunction(bridge->ctx, qjs_fn_tint, "tint", 2));
    JS_SetPropertyStr(bridge->ctx, engine_obj, "wait", JS_NewCFunction(bridge->ctx, qjs_fn_wait, "wait", 1));
    JS_SetPropertyStr(bridge->ctx, engine_obj, "nvl", JS_NewCFunction(bridge->ctx, qjs_fn_nvl, "nvl", 1));
    JS_SetPropertyStr(bridge->ctx, engine_obj, "nvlClear", JS_NewCFunction(bridge->ctx, qjs_fn_nvl_clear, "nvlClear", 0));
    JS_SetPropertyStr(bridge->ctx, engine_obj, "unlockGallery", JS_NewCFunction(bridge->ctx, qjs_fn_unlock_gallery, "unlockGallery", 1));
    JS_SetPropertyStr(bridge->ctx, engine_obj, "unlockAchievement", JS_NewCFunction(bridge->ctx, qjs_fn_unlock_achievement, "unlockAchievement", 1));
    JS_SetPropertyStr(bridge->ctx, engine_obj, "ending", JS_NewCFunction(bridge->ctx, qjs_fn_ending, "ending", 2));

    JS_SetPropertyStr(bridge->ctx, global_obj, "engine", engine_obj);
    JS_FreeValue(bridge->ctx, global_obj);

    JSValue prelude_res = JS_Eval(bridge->ctx, QJS_DSL_PRELUDE, strlen(QJS_DSL_PRELUDE), "<prelude>", JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(prelude_res)) qjs_print_exception(bridge->ctx, "<prelude>");
    JS_FreeValue(bridge->ctx, prelude_res);
}

static int qjs_init_runtime(QjsBridge* bridge) {
    bridge->rt = JS_NewRuntime();
    if (!bridge->rt) return 0;
    bridge->ctx = JS_NewContext(bridge->rt);
    if (!bridge->ctx) {
        JS_FreeRuntime(bridge->rt);
        bridge->rt = NULL;
        return 0;
    }
    JS_SetContextOpaque(bridge->ctx, bridge);
    qjs_register_bindings(bridge);
    return 1;
}

static void qjs_shutdown_runtime(QjsBridge* bridge) {
    qjs_clear_callbacks(bridge);
    if (bridge->ctx) {
        JS_FreeContext(bridge->ctx);
        bridge->ctx = NULL;
    }
    if (bridge->rt) {
        JS_FreeRuntime(bridge->rt);
        bridge->rt = NULL;
    }
}

QjsBridge* qjs_create(QjsCommandCallback cb, void* userdata) {
    QjsBridge* bridge = (QjsBridge*)calloc(1, sizeof(QjsBridge));
    if (!bridge) return NULL;
    bridge->callback = cb;
    bridge->userdata = userdata;
    bridge->next_callback_id = 1;
    if (!qjs_init_runtime(bridge)) {
        free(bridge);
        return NULL;
    }
    return bridge;
}

void qjs_destroy(QjsBridge* bridge) {
    if (!bridge) return;
    qjs_shutdown_runtime(bridge);
    free(bridge);
}

int qjs_reset(QjsBridge* bridge) {
    if (!bridge) return 0;
    qjs_shutdown_runtime(bridge);
    return qjs_init_runtime(bridge);
}

void qjs_set_language(QjsBridge* bridge, const char* lang) {
    if (!bridge || !bridge->ctx || !lang) return;
    JSValue global_obj = JS_GetGlobalObject(bridge->ctx);
    JSValue engine_obj = JS_GetPropertyStr(bridge->ctx, global_obj, "engine");
    JS_SetPropertyStr(bridge->ctx, engine_obj, "lang", JS_NewString(bridge->ctx, lang));
    JS_FreeValue(bridge->ctx, engine_obj);
    JS_FreeValue(bridge->ctx, global_obj);
}

void qjs_set_state_var(QjsBridge* bridge, const char* key, const char* val) {
    if (!bridge || !bridge->ctx || !key || !val) return;
    JSValue global_obj = JS_GetGlobalObject(bridge->ctx);
    JSValue engine_obj = JS_GetPropertyStr(bridge->ctx, global_obj, "engine");
    JSValue state_obj = JS_GetPropertyStr(bridge->ctx, engine_obj, "state");
    JS_SetPropertyStr(bridge->ctx, state_obj, key, JS_NewString(bridge->ctx, val));
    JS_FreeValue(bridge->ctx, state_obj);
    JS_FreeValue(bridge->ctx, engine_obj);
    JS_FreeValue(bridge->ctx, global_obj);
}

char* qjs_get_state_var(QjsBridge* bridge, const char* key) {
    if (!bridge || !bridge->ctx || !key) return NULL;
    JSValue global_obj = JS_GetGlobalObject(bridge->ctx);
    JSValue engine_obj = JS_GetPropertyStr(bridge->ctx, global_obj, "engine");
    JSValue state_obj = JS_GetPropertyStr(bridge->ctx, engine_obj, "state");
    JSValue val = JS_GetPropertyStr(bridge->ctx, state_obj, key);
    char* out = NULL;
    if (!JS_IsUndefined(val) && !JS_IsNull(val)) {
        const char* cstr = JS_ToCString(bridge->ctx, val);
        if (cstr) {
            size_t len = strlen(cstr);
            out = (char*)malloc(len + 1);
            if (out) memcpy(out, cstr, len + 1);
            JS_FreeCString(bridge->ctx, cstr);
        }
    }
    JS_FreeValue(bridge->ctx, val);
    JS_FreeValue(bridge->ctx, state_obj);
    JS_FreeValue(bridge->ctx, engine_obj);
    JS_FreeValue(bridge->ctx, global_obj);
    return out;
}

int qjs_execute_code(QjsBridge* bridge, const char* code, size_t code_len, const char* filename) {
    if (!bridge || !bridge->ctx || !code) return 0;
    JSValue val = JS_Eval(bridge->ctx, code, code_len, filename ? filename : "<script>", JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(val)) {
        qjs_print_exception(bridge->ctx, filename ? filename : "<script>");
        JS_FreeValue(bridge->ctx, val);
        return 0;
    }
    JS_FreeValue(bridge->ctx, val);
    return 1;
}

int qjs_invoke_callback(QjsBridge* bridge, int callback_id, int choice_index) {
    if (!bridge || !bridge->ctx) return 0;
    size_t found = bridge->slot_count;
    for (size_t i = 0; i < bridge->slot_count; ++i) {
        if (bridge->slots[i].id == callback_id) {
            found = i;
            break;
        }
    }
    if (found == bridge->slot_count) return 0;

    JSValue func = bridge->slots[found].func;
    bridge->slots[found] = bridge->slots[bridge->slot_count - 1];
    bridge->slot_count--;

    JSValue arg = JS_NewInt32(bridge->ctx, choice_index);
    JSValue ret = JS_Call(bridge->ctx, func, JS_UNDEFINED, 1, &arg);
    int ok = 1;
    if (JS_IsException(ret)) {
        qjs_print_exception(bridge->ctx, "callback");
        ok = 0;
    }
    JS_FreeValue(bridge->ctx, ret);
    JS_FreeValue(bridge->ctx, arg);
    JS_FreeValue(bridge->ctx, func);
    return ok;
}

char* qjs_serialize_state(QjsBridge* bridge) {
    if (!bridge || !bridge->ctx) return NULL;
    JSContext* ctx = bridge->ctx;
    JSValue global_obj = JS_GetGlobalObject(ctx);
    JSValue engine_obj = JS_GetPropertyStr(ctx, global_obj, "engine");
    JSValue state_val = JS_GetPropertyStr(ctx, engine_obj, "state");
    JSValue json_obj = JS_GetPropertyStr(ctx, global_obj, "JSON");
    JSValue stringify_fn = JS_GetPropertyStr(ctx, json_obj, "stringify");

    char* result = NULL;
    if (JS_IsFunction(ctx, stringify_fn)) {
        JSValue str_val = JS_Call(ctx, stringify_fn, json_obj, 1, &state_val);
        if (!JS_IsException(str_val) && JS_IsString(str_val)) {
            const char* cstr = JS_ToCString(ctx, str_val);
            if (cstr) {
                size_t len = strlen(cstr);
                result = (char*)malloc(len + 1);
                if (result) memcpy(result, cstr, len + 1);
                JS_FreeCString(ctx, cstr);
            }
        } else if (JS_IsException(str_val)) {
            qjs_print_exception(ctx, "JSON.stringify");
        }
        JS_FreeValue(ctx, str_val);
    }

    JS_FreeValue(ctx, stringify_fn);
    JS_FreeValue(ctx, json_obj);
    JS_FreeValue(ctx, state_val);
    JS_FreeValue(ctx, engine_obj);
    JS_FreeValue(ctx, global_obj);
    return result;
}

int qjs_deserialize_state(QjsBridge* bridge, const char* json_str) {
    if (!bridge || !bridge->ctx || !json_str) return 0;
    JSContext* ctx = bridge->ctx;
    JSValue global_obj = JS_GetGlobalObject(ctx);
    JSValue engine_obj = JS_GetPropertyStr(ctx, global_obj, "engine");
    JSValue json_obj = JS_GetPropertyStr(ctx, global_obj, "JSON");
    JSValue parse_fn = JS_GetPropertyStr(ctx, json_obj, "parse");

    int ok = 0;
    if (JS_IsFunction(ctx, parse_fn)) {
        JSValue arg = JS_NewString(ctx, json_str);
        JSValue parsed = JS_Call(ctx, parse_fn, json_obj, 1, &arg);
        if (!JS_IsException(parsed)) {
            JS_SetPropertyStr(ctx, engine_obj, "state", parsed);
            ok = 1;
        } else {
            qjs_print_exception(ctx, "JSON.parse");
            JS_FreeValue(ctx, parsed);
        }
        JS_FreeValue(ctx, arg);
    }

    JS_FreeValue(ctx, parse_fn);
    JS_FreeValue(ctx, json_obj);
    JS_FreeValue(ctx, engine_obj);
    JS_FreeValue(ctx, global_obj);
    return ok;
}

void qjs_free_cstr(char* str) {
    free(str);
}