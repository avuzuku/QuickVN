#pragma once
#include <SFML/Graphics.hpp>
#include <optional>
#include <string>
#include <vector>

struct ChoiceButton {
    sf::RectangleShape box;
    std::optional<sf::Text> text;
};

class Scene {
public:
    Scene() = default;

    bool init(const std::string& fontPath);
    bool setBackground(const std::string& path);
    bool setCharacter(const std::string& path);
    void setDialogue(const std::string& name, const std::string& text);

    // Choice UI controls
    void setChoices(const std::vector<std::string>& choices);
    void updateChoiceHover(const sf::Vector2f& mousePos);
    int getChoiceAt(const sf::Vector2f& mousePos);
    void clearChoices();
    bool hasChoices() const { return m_hasChoices; }

    void render(sf::RenderWindow& window, bool showDialogue);

private:
    sf::Font m_font;
    std::optional<sf::Text> m_textName;
    std::optional<sf::Text> m_textDialogue;
    sf::RectangleShape m_textBox;

    sf::Texture m_bgTexture;
    std::optional<sf::Sprite> m_bgSprite;
    bool m_hasBg = false;

    sf::Texture m_charTexture;
    std::optional<sf::Sprite> m_charSprite;
    bool m_hasChar = false;

    std::vector<ChoiceButton> m_choiceButtons;
    bool m_hasChoices = false;
};