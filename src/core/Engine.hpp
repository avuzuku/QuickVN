#pragma once
#include <SFML/Graphics.hpp>
#include <vector>
#include "core/Command.hpp"
#include "graphics/Scene.hpp"
#include "audio/AudioManager.hpp"
#include "script/ScriptManager.hpp"

class Engine {
public:
    Engine() = default;
    ~Engine() = default;

    bool init();
    void run();

private:
    void processEvents();
    void update();
    void render();
    void executeNextCommand();

    sf::RenderWindow m_window;
    Scene m_scene;
    AudioManager m_audioManager;
    ScriptManager m_scriptManager;

    std::vector<Command> m_commands;
    size_t m_currentCommandIdx = 0;
    bool m_waitingForClick = false;

    bool m_waitingForChoice = false;
    int m_currentChoiceCallbackId = -1;
};