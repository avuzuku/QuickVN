#include "Engine.hpp"
#include <iostream>
#include <fstream>
#include <sstream>

static Engine* g_engineInstance = nullptr;

// --- QuickJS C-API Callbacks ---

static JSValue js_say(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    if (argc < 2) return JS_EXCEPTION;
    
    const char* name = JS_ToCString(ctx, argv[0]);
    const char* text = JS_ToCString(ctx, argv[1]);
    
    if (g_engineInstance && name && text) {
        g_engineInstance->addCommand({CommandType::Say, name, text});
    }
    
    JS_FreeCString(ctx, name);
    JS_FreeCString(ctx, text);
    return JS_UNDEFINED;
}

static JSValue js_showBackground(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    if (argc < 1) return JS_EXCEPTION;
    const char* path = JS_ToCString(ctx, argv[0]);
    
    if (g_engineInstance && path) {
        g_engineInstance->addCommand({CommandType::ShowBackground, path, ""});
    }
    
    JS_FreeCString(ctx, path);
    return JS_UNDEFINED;
}

static JSValue js_showCharacter(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    if (argc < 1) return JS_EXCEPTION;
    const char* path = JS_ToCString(ctx, argv[0]);
    
    if (g_engineInstance && path) {
        g_engineInstance->addCommand({CommandType::ShowCharacter, path, ""});
    }
    
    JS_FreeCString(ctx, path);
    return JS_UNDEFINED;
}

void setupQuickJSBindings(JSContext* ctx) {
    JSValue global_obj = JS_GetGlobalObject(ctx);
    JSValue engine_obj = JS_NewObject(ctx);
    
    JS_SetPropertyStr(ctx, engine_obj, "say", JS_NewCFunction(ctx, js_say, "say", 2));
    JS_SetPropertyStr(ctx, engine_obj, "showBackground", JS_NewCFunction(ctx, js_showBackground, "showBackground", 1));
    JS_SetPropertyStr(ctx, engine_obj, "showCharacter", JS_NewCFunction(ctx, js_showCharacter, "showCharacter", 1));
    
    JS_SetPropertyStr(ctx, global_obj, "engine", engine_obj);
    JS_FreeValue(ctx, global_obj);
}

// --- Engine Class Implementation ---

Engine::Engine() {
    g_engineInstance = this;
}

Engine::~Engine() {
    if (m_ctx) JS_FreeContext(m_ctx);
    if (m_rt) JS_FreeRuntime(m_rt);
    g_engineInstance = nullptr;
}

bool Engine::init() {
    m_window.create(sf::VideoMode({1280, 720}), "QuickVN Engine");
    m_window.setFramerateLimit(60);

    // SFML 3: Font stream loader
    if (!m_font.openFromFile("assets/font.ttf")) {
        std::cerr << "Error: Could not load assets/font.ttf" << std::endl;
        return false;
    }

    m_textBox.setSize({1200.f, 160.f});
    m_textBox.setFillColor(sf::Color(0, 0, 0, 220));
    m_textBox.setOutlineColor(sf::Color::White);
    m_textBox.setOutlineThickness(2.f);
    m_textBox.setPosition({40.f, 520.f});

    m_textName.emplace(m_font, "", 24);
    m_textName->setFillColor(sf::Color::Yellow);
    m_textName->setPosition({60.f, 530.f});

    m_textDialogue.emplace(m_font, "", 20);
    m_textDialogue->setFillColor(sf::Color::White);
    m_textDialogue->setPosition({60.f, 570.f});

    m_rt = JS_NewRuntime();
    if (!m_rt) return false;
    
    m_ctx = JS_NewContext(m_rt);
    if (!m_ctx) return false;

    setupQuickJSBindings(m_ctx);

    if (!loadScript("scripts/main.js")) {
        return false;
    }

    executeNextCommand();
    return true;
}

bool Engine::loadScript(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "Failed to open script: " << filepath << std::endl;
        return false;
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string code = buffer.str();

    JSValue val = JS_Eval(m_ctx, code.c_str(), code.size(), filepath.c_str(), JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(val)) {
        JSValue exception = JS_GetException(m_ctx);
        const char* err_str = JS_ToCString(m_ctx, exception);
        std::cerr << "JS Error in " << filepath << ": " << err_str << std::endl;
        JS_FreeCString(m_ctx, err_str);
        JS_FreeValue(m_ctx, exception);
        JS_FreeValue(m_ctx, val);
        return false;
    }
    JS_FreeValue(m_ctx, val);
    return true;
}

void Engine::addCommand(Command cmd) {
    m_commands.push_back(cmd);
}

void Engine::executeNextCommand() {
    if (m_currentCommandIdx >= m_commands.size()) {
        m_waitingForClick = false;
        return;
    }

    m_waitingForClick = false;

    while (m_currentCommandIdx < m_commands.size() && !m_waitingForClick) {
        const Command& cmd = m_commands[m_currentCommandIdx];
        m_currentCommandIdx++;

        if (cmd.type == CommandType::ShowBackground) {
            if (m_bgTexture.loadFromFile(cmd.arg1)) {
                m_bgSprite.emplace(m_bgTexture);
                sf::Vector2u size = m_bgTexture.getSize();
                m_bgSprite->setScale({1280.f / size.x, 720.f / size.y});
                m_hasBg = true;
            }
        } 
        else if (cmd.type == CommandType::ShowCharacter) {
            if (m_charTexture.loadFromFile(cmd.arg1)) {
                m_charSprite.emplace(m_charTexture);
                sf::Vector2u size = m_charTexture.getSize();
                m_charSprite->setOrigin({size.x / 2.f, static_cast<float>(size.y)});
                m_charSprite->setPosition({1280.f / 2.f, 720.f});
                m_hasChar = true;
            }
        } 
        else if (cmd.type == CommandType::Say) {
            if (m_textName && m_textDialogue) {
                m_textName->setString(sf::String::fromUtf8(cmd.arg1.begin(), cmd.arg1.end()));
                m_textDialogue->setString(sf::String::fromUtf8(cmd.arg2.begin(), cmd.arg2.end()));
            }
            m_waitingForClick = true;
        }
    }
}

void Engine::run() {
    while (m_window.isOpen()) {
        processEvents();
        update();
        render();
    }
}

void Engine::processEvents() {
    while (const std::optional event = m_window.pollEvent()) {
        if (event->is<sf::Event::Closed>()) {
            m_window.close();
        }
        else if (const auto* mouseButton = event->getIf<sf::Event::MouseButtonPressed>()) {
            if (mouseButton->button == sf::Mouse::Button::Left && m_waitingForClick) {
                executeNextCommand();
            }
        }
        else if (const auto* keyPressed = event->getIf<sf::Event::KeyPressed>()) {
            if ((keyPressed->code == sf::Keyboard::Key::Space || keyPressed->code == sf::Keyboard::Key::Enter) && m_waitingForClick) {
                executeNextCommand();
            }
        }
    }
}

void Engine::update() {}

void Engine::render() {
    m_window.clear();

    if (m_hasBg && m_bgSprite) m_window.draw(*m_bgSprite);
    if (m_hasChar && m_charSprite) m_window.draw(*m_charSprite);

    if (m_waitingForClick) {
        m_window.draw(m_textBox);
        if (m_textName) m_window.draw(*m_textName);
        if (m_textDialogue) m_window.draw(*m_textDialogue);
    }

    m_window.display();
}