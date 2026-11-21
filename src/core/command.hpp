#pragma once
#include <string>
#include <vector>

enum class CommandType {
    ShowBackground,
    ShowCharacter,
    MoveCharacter,
    HideCharacter,
    HideAllCharacters,
    ScaleCharacter,
    BounceCharacter,
    FlipCharacter,
    Say,
    PlayMusic,
    StopMusic,
    PlaySound,
    PlayVoice,
    ShowChoices,
    TimedChoice,
    PromptInput,
    Chapter,
    Notify,
    SaveState,
    Autosave,
    LoadState,
    Shake,
    Flash,
    Tint,
    CamZoom,
    CamPan,
    Weather,
    Cinema,
    Vignette,
    Wait,
    NvlMode,
    NvlClear,
    UnlockGallery,
    UnlockAchievement,
    Ending
};

struct Command {
    CommandType type = CommandType::Say;
    std::string arg1;
    std::string arg2;
    std::string arg3;
    std::string arg4;
    std::vector<std::string> choices;
    int callback_id = -1;
    float num1 = 0.f;
    float num2 = 0.f;
    float duration1 = -1.f;
    float duration2 = -1.f;
    int int_val = 0;
    bool flag = false;
};