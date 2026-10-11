#include "core/Engine.hpp"
#include <iostream>

bool Engine::init() {
    m_window.create(sf::VideoMode({1280, 720}), "QuickVN Engine");
    m_window.setFramerateLimit(60);

    if (!m_scene.init("assets/font.ttf")) {
        return false;
    }

    bool scriptInitOk = m_scriptManager.init([this](const Command& cmd) {
        m_commands.push_back(cmd);
    });

    if (!scriptInitOk) {
        std::cerr << "Engine: Failed to initialize ScriptManager." << std::endl;
        return false;
    }

    if (!m_scriptManager.executeFile("scripts/main.js")) {
        return false;
    }

    executeNextCommand();
    return true;
}

void Engine::executeNextCommand() {
    if (m_currentCommandIdx >= m_commands.size()) {
        m_waitingForClick = false;
        return;
    }

    m_waitingForClick = false;

    while (m_currentCommandIdx < m_commands.size() && !m_waitingForClick && !m_waitingForChoice) {
        const Command& cmd = m_commands[m_currentCommandIdx++];

        if (cmd.type == CommandType::ShowBackground) {
            m_scene.setBackground(cmd.arg1);
        } 
        else if (cmd.type == CommandType::ShowCharacter) {
            m_scene.setCharacter(cmd.arg1);
        } 
        else if (cmd.type == CommandType::PlayMusic) {
            bool loop = (cmd.arg2 == "true");
            m_audioManager.playMusic(cmd.arg1, loop);
        }
        else if (cmd.type == CommandType::StopMusic) {
            m_audioManager.stopMusic();
        }
        else if (cmd.type == CommandType::PlaySound) {
            m_audioManager.playSound(cmd.arg1);
        }
        else if (cmd.type == CommandType::Say) {
            m_scene.setDialogue(cmd.arg1, cmd.arg2);
            m_waitingForClick = true;
        }
        else if (cmd.type == CommandType::ShowChoices) {
            m_scene.setChoices(cmd.choices);
            m_currentChoiceCallbackId = cmd.callbackId;
            m_waitingForChoice = true;
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
        else if (const auto* mouseMoved = event->getIf<sf::Event::MouseMoved>()) {
            if (m_waitingForChoice) {
                sf::Vector2f mousePos{static_cast<float>(mouseMoved->position.x), static_cast<float>(mouseMoved->position.y)};
                m_scene.updateChoiceHover(mousePos);
            }
        }
        else if (const auto* mouseButton = event->getIf<sf::Event::MouseButtonPressed>()) {
            if (mouseButton->button == sf::Mouse::Button::Left) {
                if (m_waitingForChoice) {
                    sf::Vector2f mousePos{static_cast<float>(mouseButton->position.x), static_cast<float>(mouseButton->position.y)};
                    int selected = m_scene.getChoiceAt(mousePos);
                    if (selected >= 0) {
                        m_waitingForChoice = false;
                        m_scene.clearChoices();
                        int cbId = m_currentChoiceCallbackId;
                        m_currentChoiceCallbackId = -1;
                        m_scriptManager.invokeCallback(cbId, selected);
                        executeNextCommand();
                    }
                }
                else if (m_waitingForClick) {
                    executeNextCommand();
                }
            }
        }
        else if (const auto* keyPressed = event->getIf<sf::Event::KeyPressed>()) {
            if ((keyPressed->code == sf::Keyboard::Key::Space || keyPressed->code == sf::Keyboard::Key::Enter)
                && m_waitingForClick && !m_waitingForChoice) {
                executeNextCommand();
            }
        }
    }
}

void Engine::update() {
    m_audioManager.update();
}

void Engine::render() {
    m_window.clear();
    m_scene.render(m_window, m_waitingForClick || m_waitingForChoice);
    m_window.display();
}