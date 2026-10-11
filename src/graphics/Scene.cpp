#include "graphics/Scene.hpp"
#include <iostream>

bool Scene::init(const std::string& fontPath) {
    if (!m_font.openFromFile(fontPath)) {
        std::cerr << "Scene Error: Failed to open font " << fontPath << std::endl;
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

    return true;
}

bool Scene::setBackground(const std::string& path) {
    if (m_bgTexture.loadFromFile(path)) {
        m_bgSprite.emplace(m_bgTexture);
        sf::Vector2u size = m_bgTexture.getSize();
        m_bgSprite->setScale({1280.f / size.x, 720.f / size.y});
        m_hasBg = true;
        return true;
    }
    std::cerr << "Scene Error: Failed to load background " << path << std::endl;
    return false;
}

bool Scene::setCharacter(const std::string& path) {
    if (m_charTexture.loadFromFile(path)) {
        m_charSprite.emplace(m_charTexture);
        sf::Vector2u size = m_charTexture.getSize();
        m_charSprite->setOrigin({size.x / 2.f, static_cast<float>(size.y)});
        m_charSprite->setPosition({1280.f / 2.f, 720.f});
        m_hasChar = true;
        return true;
    }
    std::cerr << "Scene Error: Failed to load character " << path << std::endl;
    return false;
}

void Scene::setDialogue(const std::string& name, const std::string& text) {
    if (m_textName && m_textDialogue) {
        m_textName->setString(sf::String::fromUtf8(name.begin(), name.end()));
        m_textDialogue->setString(sf::String::fromUtf8(text.begin(), text.end()));
    }
}

void Scene::setChoices(const std::vector<std::string>& choices) {
    m_choiceButtons.clear();
    m_hasChoices = !choices.empty();
    if (!m_hasChoices) return;

    const float btnWidth = 600.f;
    const float btnHeight = 50.f;
    const float spacing = 15.f;
    const float totalHeight = choices.size() * btnHeight + (choices.size() - 1) * spacing;
    const float startY = 260.f - (totalHeight / 2.f);
    const float startX = (1280.f - btnWidth) / 2.f;

    for (size_t i = 0; i < choices.size(); ++i) {
        ChoiceButton btn;
        float y = startY + i * (btnHeight + spacing);

        btn.box.setSize({btnWidth, btnHeight});
        btn.box.setPosition({startX, y});
        btn.box.setFillColor(sf::Color(25, 25, 30, 230));
        btn.box.setOutlineColor(sf::Color::White);
        btn.box.setOutlineThickness(2.f);

        btn.text.emplace(m_font, sf::String::fromUtf8(choices[i].begin(), choices[i].end()), 22);
        btn.text->setFillColor(sf::Color::White);

        // Center text horizontally and vertically inside the button rectangle
        sf::FloatRect textBounds = btn.text->getLocalBounds();
        float textX = startX + (btnWidth - textBounds.size.x) / 2.f - textBounds.position.x;
        float textY = y + (btnHeight - textBounds.size.y) / 2.f - textBounds.position.y;
        btn.text->setPosition({textX, textY});

        m_choiceButtons.push_back(std::move(btn));
    }
}

void Scene::updateChoiceHover(const sf::Vector2f& mousePos) {
    for (auto& btn : m_choiceButtons) {
        if (btn.box.getGlobalBounds().contains(mousePos)) {
            btn.box.setFillColor(sf::Color(55, 65, 110, 245));
            btn.box.setOutlineColor(sf::Color::Yellow);
        } else {
            btn.box.setFillColor(sf::Color(25, 25, 30, 230));
            btn.box.setOutlineColor(sf::Color::White);
        }
    }
}

int Scene::getChoiceAt(const sf::Vector2f& mousePos) {
    for (size_t i = 0; i < m_choiceButtons.size(); ++i) {
        if (m_choiceButtons[i].box.getGlobalBounds().contains(mousePos)) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

void Scene::clearChoices() {
    m_choiceButtons.clear();
    m_hasChoices = false;
}

void Scene::render(sf::RenderWindow& window, bool showDialogue) {
    if (m_hasBg && m_bgSprite) {
        window.draw(*m_bgSprite);
    }
    if (m_hasChar && m_charSprite) {
        window.draw(*m_charSprite);
    }

    if (showDialogue) {
        window.draw(m_textBox);
        if (m_textName) window.draw(*m_textName);
        if (m_textDialogue) window.draw(*m_textDialogue);
    }

    if (m_hasChoices) {
        for (const auto& btn : m_choiceButtons) {
            window.draw(btn.box);
            if (btn.text) window.draw(*btn.text);
        }
    }
}