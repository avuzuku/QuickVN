#pragma once
#include "core/Command.hpp"
#include <functional>
#include <string>
#include <vector>
#include <unordered_map>

extern "C" {
#ifdef __has_include
    #if __has_include(<quickjs/quickjs.h>)
        #include <quickjs/quickjs.h>
    #else
        #include <quickjs.h>
    #endif
#else
    #include <quickjs.h>
#endif
}

class ScriptManager {
public:
    using CommandDispatcher = std::function<void(const Command&)>;

    ScriptManager();
    ~ScriptManager();

    bool init(CommandDispatcher dispatcher);
    bool executeFile(const std::string& filepath);
    void invokeCallback(int id, int choiceIndex);

private:
    void registerBindings();
    int storeCallback(JSValueConst func);

    JSRuntime* m_rt = nullptr;
    JSContext* m_ctx = nullptr;
    CommandDispatcher m_dispatcher;

    int m_nextCallbackId = 1;
    std::unordered_map<int, JSValue> m_callbacks;

    static ScriptManager* s_instance;
    static JSValue js_say(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
    static JSValue js_showBackground(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
    static JSValue js_showCharacter(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
    static JSValue js_playMusic(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
    static JSValue js_stopMusic(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
    static JSValue js_playSound(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
    static JSValue js_showChoices(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
};