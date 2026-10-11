#include "script/ScriptManager.hpp"
#include <iostream>
#include <fstream>
#include <sstream>

ScriptManager* ScriptManager::s_instance = nullptr;

ScriptManager::ScriptManager() {
    s_instance = this;
}

ScriptManager::~ScriptManager() {
    if (m_ctx) {
        for (auto& [id, func] : m_callbacks) {
            JS_FreeValue(m_ctx, func);
        }
        m_callbacks.clear();
        JS_FreeContext(m_ctx);
    }
    if (m_rt) JS_FreeRuntime(m_rt);
    s_instance = nullptr;
}

bool ScriptManager::init(CommandDispatcher dispatcher) {
    m_dispatcher = std::move(dispatcher);

    m_rt = JS_NewRuntime();
    if (!m_rt) return false;

    m_ctx = JS_NewContext(m_rt);
    if (!m_ctx) return false;

    registerBindings();
    return true;
}

void ScriptManager::registerBindings() {
    JSValue global_obj = JS_GetGlobalObject(m_ctx);
    JSValue engine_obj = JS_NewObject(m_ctx);

    JS_SetPropertyStr(m_ctx, engine_obj, "say", JS_NewCFunction(m_ctx, js_say, "say", 2));
    JS_SetPropertyStr(m_ctx, engine_obj, "showBackground", JS_NewCFunction(m_ctx, js_showBackground, "showBackground", 1));
    JS_SetPropertyStr(m_ctx, engine_obj, "showCharacter", JS_NewCFunction(m_ctx, js_showCharacter, "showCharacter", 1));
    JS_SetPropertyStr(m_ctx, engine_obj, "playMusic", JS_NewCFunction(m_ctx, js_playMusic, "playMusic", 2));
    JS_SetPropertyStr(m_ctx, engine_obj, "stopMusic", JS_NewCFunction(m_ctx, js_stopMusic, "stopMusic", 0));
    JS_SetPropertyStr(m_ctx, engine_obj, "playSound", JS_NewCFunction(m_ctx, js_playSound, "playSound", 1));
    JS_SetPropertyStr(m_ctx, engine_obj, "showChoices", JS_NewCFunction(m_ctx, js_showChoices, "showChoices", 2));

    JS_SetPropertyStr(m_ctx, global_obj, "engine", engine_obj);
    JS_FreeValue(m_ctx, global_obj);
}

int ScriptManager::storeCallback(JSValueConst func) {
    int id = m_nextCallbackId++;
    m_callbacks[id] = JS_DupValue(m_ctx, func);
    return id;
}

void ScriptManager::invokeCallback(int id, int choiceIndex) {
    auto it = m_callbacks.find(id);
    if (it == m_callbacks.end()) return;

    JSValue func = it->second;
    m_callbacks.erase(it);

    JSValue arg = JS_NewInt32(m_ctx, choiceIndex);
    JSValue ret = JS_Call(m_ctx, func, JS_UNDEFINED, 1, &arg);
    if (JS_IsException(ret)) {
        JSValue exception = JS_GetException(m_ctx);
        const char* err_str = JS_ToCString(m_ctx, exception);
        std::cerr << "ScriptManager Callback JS Error: " << err_str << std::endl;
        JS_FreeCString(m_ctx, err_str);
        JS_FreeValue(m_ctx, exception);
    }
    JS_FreeValue(m_ctx, ret);
    JS_FreeValue(m_ctx, arg);
    JS_FreeValue(m_ctx, func);
}

bool ScriptManager::executeFile(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "ScriptManager: Cannot open file " << filepath << std::endl;
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string code = buffer.str();

    JSValue val = JS_Eval(m_ctx, code.c_str(), code.size(), filepath.c_str(), JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(val)) {
        JSValue exception = JS_GetException(m_ctx);
        const char* err_str = JS_ToCString(m_ctx, exception);
        std::cerr << "ScriptManager JS Error in " << filepath << ": " << err_str << std::endl;
        JS_FreeCString(m_ctx, err_str);
        JS_FreeValue(m_ctx, exception);
        JS_FreeValue(m_ctx, val);
        return false;
    }
    JS_FreeValue(m_ctx, val);
    return true;
}

JSValue ScriptManager::js_say(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    if (argc < 2) return JS_EXCEPTION;
    const char* name = JS_ToCString(ctx, argv[0]);
    const char* text = JS_ToCString(ctx, argv[1]);

    if (s_instance && s_instance->m_dispatcher && name && text) {
        s_instance->m_dispatcher({CommandType::Say, name, text});
    }

    JS_FreeCString(ctx, name);
    JS_FreeCString(ctx, text);
    return JS_UNDEFINED;
}

JSValue ScriptManager::js_showBackground(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    if (argc < 1) return JS_EXCEPTION;
    const char* path = JS_ToCString(ctx, argv[0]);

    if (s_instance && s_instance->m_dispatcher && path) {
        s_instance->m_dispatcher({CommandType::ShowBackground, path, ""});
    }

    JS_FreeCString(ctx, path);
    return JS_UNDEFINED;
}

JSValue ScriptManager::js_showCharacter(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    if (argc < 1) return JS_EXCEPTION;
    const char* path = JS_ToCString(ctx, argv[0]);

    if (s_instance && s_instance->m_dispatcher && path) {
        s_instance->m_dispatcher({CommandType::ShowCharacter, path, ""});
    }

    JS_FreeCString(ctx, path);
    return JS_UNDEFINED;
}

JSValue ScriptManager::js_playMusic(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    if (argc < 1) return JS_EXCEPTION;
    const char* path = JS_ToCString(ctx, argv[0]);
    bool loop = true;
    if (argc >= 2) loop = JS_ToBool(ctx, argv[1]);

    if (s_instance && s_instance->m_dispatcher && path) {
        s_instance->m_dispatcher({CommandType::PlayMusic, path, loop ? "true" : "false"});
    }

    JS_FreeCString(ctx, path);
    return JS_UNDEFINED;
}

JSValue ScriptManager::js_stopMusic(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    if (s_instance && s_instance->m_dispatcher) {
        s_instance->m_dispatcher({CommandType::StopMusic, "", ""});
    }
    return JS_UNDEFINED;
}

JSValue ScriptManager::js_playSound(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    if (argc < 1) return JS_EXCEPTION;
    const char* path = JS_ToCString(ctx, argv[0]);

    if (s_instance && s_instance->m_dispatcher && path) {
        s_instance->m_dispatcher({CommandType::PlaySound, path, ""});
    }

    JS_FreeCString(ctx, path);
    return JS_UNDEFINED;
}

JSValue ScriptManager::js_showChoices(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    if (argc < 2) return JS_EXCEPTION;
    if (!JS_IsArray(ctx, argv[0]) || !JS_IsFunction(ctx, argv[1])) return JS_EXCEPTION;

    JSValue lenVal = JS_GetPropertyStr(ctx, argv[0], "length");
    uint32_t len = 0;
    JS_ToUint32(ctx, &len, lenVal);
    JS_FreeValue(ctx, lenVal);

    std::vector<std::string> choices;
    for (uint32_t i = 0; i < len; ++i) {
        JSValue item = JS_GetPropertyUint32(ctx, argv[0], i);
        const char* str = JS_ToCString(ctx, item);
        if (str) {
            choices.emplace_back(str);
            JS_FreeCString(ctx, str);
        }
        JS_FreeValue(ctx, item);
    }

    if (s_instance && s_instance->m_dispatcher) {
        int callbackId = s_instance->storeCallback(argv[1]);
        Command cmd;
        cmd.type = CommandType::ShowChoices;
        cmd.choices = std::move(choices);
        cmd.callbackId = callbackId;
        s_instance->m_dispatcher(cmd);
    }

    return JS_UNDEFINED;
}