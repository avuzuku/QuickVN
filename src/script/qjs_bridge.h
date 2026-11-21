#pragma once
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum QjsCommandType {
    QJS_CMD_SHOW_BACKGROUND = 0,
    QJS_CMD_SHOW_CHARACTER,
    QJS_CMD_MOVE_CHARACTER,
    QJS_CMD_HIDE_CHARACTER,
    QJS_CMD_HIDE_ALL_CHARACTERS,
    QJS_CMD_SCALE_CHARACTER,
    QJS_CMD_BOUNCE_CHARACTER,
    QJS_CMD_FLIP_CHARACTER,
    QJS_CMD_SAY,
    QJS_CMD_PLAY_MUSIC,
    QJS_CMD_STOP_MUSIC,
    QJS_CMD_PLAY_SOUND,
    QJS_CMD_PLAY_VOICE,
    QJS_CMD_SHOW_CHOICES,
    QJS_CMD_TIMED_CHOICE,
    QJS_CMD_PROMPT_INPUT,
    QJS_CMD_CHAPTER,
    QJS_CMD_NOTIFY,
    QJS_CMD_SAVE_STATE,
    QJS_CMD_AUTOSAVE,
    QJS_CMD_LOAD_STATE,
    QJS_CMD_SHAKE,
    QJS_CMD_FLASH,
    QJS_CMD_TINT,
    QJS_CMD_CAM_ZOOM,
    QJS_CMD_CAM_PAN,
    QJS_CMD_WEATHER,
    QJS_CMD_CINEMA,
    QJS_CMD_VIGNETTE,
    QJS_CMD_WAIT,
    QJS_CMD_NVL_MODE,
    QJS_CMD_NVL_CLEAR,
    QJS_CMD_UNLOCK_GALLERY,
    QJS_CMD_UNLOCK_ACHIEVEMENT,
    QJS_CMD_ENDING
} QjsCommandType;

typedef struct QjsCommandView {
    QjsCommandType type;
    const char* arg1;
    const char* arg2;
    const char* arg3;
    const char* arg4;
    const char* const* choices;
    size_t choice_count;
    int callback_id;
    float num1;
    float num2;
    float duration1;
    float duration2;
    int flag;
} QjsCommandView;

typedef void (*QjsCommandCallback)(const QjsCommandView* cmd, void* userdata);

typedef struct QjsBridge QjsBridge;

QjsBridge* qjs_create(QjsCommandCallback cb, void* userdata);
void qjs_destroy(QjsBridge* bridge);
int qjs_reset(QjsBridge* bridge);
void qjs_set_language(QjsBridge* bridge, const char* lang);
void qjs_set_state_var(QjsBridge* bridge, const char* key, const char* val);
char* qjs_get_state_var(QjsBridge* bridge, const char* key);
int qjs_execute_code(QjsBridge* bridge, const char* code, size_t code_len, const char* filename);
int qjs_invoke_callback(QjsBridge* bridge, int callback_id, int choice_index);
char* qjs_serialize_state(QjsBridge* bridge);
int qjs_deserialize_state(QjsBridge* bridge, const char* json_str);
void qjs_free_cstr(char* str);

#ifdef __cplusplus
}
#endif