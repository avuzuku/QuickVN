#pragma once
#include "rust_core.h"
#include "ui/ui.hpp"
#include <SFML/Graphics.hpp>
#include <string>
#include <vector>

enum class ModMenuAction {
    None = 0,
    Close,
    ApplyAndReload,
    TestModCreated
};

struct ModMenuState {
    std::vector<QvnModInfo> items;
    int scroll = 0;
    bool dirty = false;
};

void mod_menu_reload(ModMenuState& menu, const std::string& base_dir);
bool mod_menu_save(ModMenuState& menu, const std::string& base_dir);
ModMenuAction mod_menu_render(
    ModMenuState& menu,
    UiContext& ui,
    sf::RenderWindow& window,
    const QvnConfig& cfg,
    const std::string& locale_json,
    const std::string& base_dir
);