#pragma once
#include <string>
#include <vector>

enum class CommandType {
    ShowBackground,
    ShowCharacter,
    Say,
    PlayMusic,
    StopMusic,
    PlaySound,
    ShowChoices
};

struct Command {
    CommandType type;
    std::string arg1;
    std::string arg2;
    std::vector<std::string> choices;
    int callbackId = -1;
};