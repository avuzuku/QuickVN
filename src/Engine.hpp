#pragma once
#include <SFML/Graphics.hpp>
#include <string>
#include <vector>
#include <optional>

// Safe QuickJS C API inclusion for C++
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

enum class CommandType {
    ShowBackground,
    ShowCharacter,
    Say
};

struct Command {
    CommandType type;
    std::string arg1;
    std::string arg2;
};

class Engine {
public:
    Engine();
    ~Engine();

    bool init();
    void run();

    void addCommand(Command cmd);

private:
    void processEvents();
    void update();
    void render();
    
    void executeNextCommand();
    bool loadScript(const std::string& filepath);

    sf::RenderWindow m_window;
    sf::Font m_font;

    // SFML 3: types without default constructors use std::optional
    std::optional<sf::Text> m_textName;
    std::optional<sf::Text> m_textDialogue;
    sf::RectangleShape m_textBox;

    sf::Texture m_bgTexture;
    std::optional<sf::Sprite> m_bgSprite;
    bool m_hasBg = false;

    sf::Texture m_charTexture;
    std::optional<sf::Sprite> m_charSprite;
    bool m_hasChar = false;

    std::vector<Command> m_commands;
    size_t m_currentCommandIdx = 0;
    bool m_waitingForClick = false;

    JSRuntime* m_rt = nullptr;
    JSContext* m_ctx = nullptr;
};