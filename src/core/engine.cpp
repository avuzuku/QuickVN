#include "core/engine.hpp"
#include "platform/platform.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <iostream>

#ifndef QVN_PAK_KEY
#define QVN_PAK_KEY "quickvn-default-secret-key-2026"
#endif

static constexpr float LOGICAL_W = 1280.f;
static constexpr float LOGICAL_H = 720.f;

static sf::Color to_sf_color(QvnColor c) {
    return sf::Color(c.r, c.g, c.b, c.a);
}

static bool ends_with_ci(const std::string& s, const std::string& suffix) {
    if (s.size() < suffix.size()) return false;
    for (size_t i = 0; i < suffix.size(); ++i) {
        char a = static_cast<char>(std::tolower(static_cast<unsigned char>(s[s.size() - suffix.size() + i])));
        char b = static_cast<char>(std::tolower(static_cast<unsigned char>(suffix[i])));
        if (a != b) return false;
    }
    return true;
}

static std::string L(EngineState& eng, const char* key, const char* fallback) {
    std::string k(key);
    auto it = eng.locale_cache.find(k);
    if (it != eng.locale_cache.end()) {
        return it->second;
    }
    std::string res = fallback ? std::string(fallback) : k;
    if (!eng.active_locale_json.empty()) {
        char* val = qvn_locale_lookup(eng.active_locale_json.c_str(), key);
        if (val) {
            res = val;
            qvn_free_cstr(val);
        }
    }
    eng.locale_cache.emplace(std::move(k), res);
    return res;
}

static void engine_load_locale(EngineState& eng) {
    eng.locale_cache.clear();
    std::string lang = eng.settings.language[0] ? std::string(eng.settings.language) : "en";
    std::string path = "locales/" + lang + ".json";
    if (!platform_read_text_file(path, eng.active_locale_json)) {
        platform_read_text_file("locales/en.json", eng.active_locale_json);
    }
    if (eng.script) qjs_set_language(eng.script, lang.c_str());
}

static void engine_capture_scene_thumb(EngineState& eng) {
    if (!eng.window.isOpen()) return;
    sf::Vector2u sz = eng.window.getSize();
    if (sz.x == 0 || sz.y == 0) return;
    sf::Texture cap;
    if (!cap.resize(sz)) return;
    cap.update(eng.window);
    sf::Image src = cap.copyToImage();
    const unsigned tw = 240;
    const unsigned th = 135;
    sf::Image dst({tw, th}, sf::Color::Black);
    for (unsigned y = 0; y < th; ++y) {
        unsigned sy = (y * sz.y) / th;
        for (unsigned x = 0; x < tw; ++x) {
            unsigned sx = (x * sz.x) / tw;
            dst.setPixel({x, y}, src.getPixel({sx, sy}));
        }
    }
    eng.last_scene_thumb = std::move(dst);
    eng.has_scene_thumb = true;
}

static void engine_on_js_command(const QjsCommandView* view, void* userdata) {
    if (!view || !userdata) return;
    EngineState* eng = static_cast<EngineState*>(userdata);
    Command cmd;
    switch (view->type) {
        case QJS_CMD_SHOW_BACKGROUND: cmd.type = CommandType::ShowBackground; break;
        case QJS_CMD_SHOW_CHARACTER: cmd.type = CommandType::ShowCharacter; break;
        case QJS_CMD_MOVE_CHARACTER: cmd.type = CommandType::MoveCharacter; break;
        case QJS_CMD_HIDE_CHARACTER: cmd.type = CommandType::HideCharacter; break;
        case QJS_CMD_HIDE_ALL_CHARACTERS: cmd.type = CommandType::HideAllCharacters; break;
        case QJS_CMD_SCALE_CHARACTER: cmd.type = CommandType::ScaleCharacter; break;
        case QJS_CMD_BOUNCE_CHARACTER: cmd.type = CommandType::BounceCharacter; break;
        case QJS_CMD_FLIP_CHARACTER: cmd.type = CommandType::FlipCharacter; break;
        case QJS_CMD_SAY: cmd.type = CommandType::Say; break;
        case QJS_CMD_PLAY_MUSIC: cmd.type = CommandType::PlayMusic; break;
        case QJS_CMD_STOP_MUSIC: cmd.type = CommandType::StopMusic; break;
        case QJS_CMD_PLAY_SOUND: cmd.type = CommandType::PlaySound; break;
        case QJS_CMD_PLAY_VOICE: cmd.type = CommandType::PlayVoice; break;
        case QJS_CMD_SHOW_CHOICES: cmd.type = CommandType::ShowChoices; break;
        case QJS_CMD_TIMED_CHOICE: cmd.type = CommandType::TimedChoice; break;
        case QJS_CMD_PROMPT_INPUT: cmd.type = CommandType::PromptInput; break;
        case QJS_CMD_CHAPTER: cmd.type = CommandType::Chapter; break;
        case QJS_CMD_NOTIFY: cmd.type = CommandType::Notify; break;
        case QJS_CMD_SAVE_STATE: cmd.type = CommandType::SaveState; break;
        case QJS_CMD_AUTOSAVE: cmd.type = CommandType::Autosave; break;
        case QJS_CMD_LOAD_STATE: cmd.type = CommandType::LoadState; break;
        case QJS_CMD_SHAKE: cmd.type = CommandType::Shake; break;
        case QJS_CMD_FLASH: cmd.type = CommandType::Flash; break;
        case QJS_CMD_TINT: cmd.type = CommandType::Tint; break;
        case QJS_CMD_CAM_ZOOM: cmd.type = CommandType::CamZoom; break;
        case QJS_CMD_CAM_PAN: cmd.type = CommandType::CamPan; break;
        case QJS_CMD_WEATHER: cmd.type = CommandType::Weather; break;
        case QJS_CMD_CINEMA: cmd.type = CommandType::Cinema; break;
        case QJS_CMD_VIGNETTE: cmd.type = CommandType::Vignette; break;
        case QJS_CMD_WAIT: cmd.type = CommandType::Wait; break;
        case QJS_CMD_NVL_MODE: cmd.type = CommandType::NvlMode; break;
        case QJS_CMD_NVL_CLEAR: cmd.type = CommandType::NvlClear; break;
        case QJS_CMD_UNLOCK_GALLERY: cmd.type = CommandType::UnlockGallery; break;
        case QJS_CMD_UNLOCK_ACHIEVEMENT: cmd.type = CommandType::UnlockAchievement; break;
        case QJS_CMD_ENDING: cmd.type = CommandType::Ending; break;
    }
    if (view->arg1) cmd.arg1 = view->arg1;
    if (view->arg2) cmd.arg2 = view->arg2;
    if (view->arg3) cmd.arg3 = view->arg3;
    if (view->arg4) cmd.arg4 = view->arg4;
    for (size_t i = 0; i < view->choice_count; ++i) {
        if (view->choices && view->choices[i]) cmd.choices.emplace_back(view->choices[i]);
    }
    cmd.callback_id = view->callback_id;
    cmd.num1 = view->num1;
    cmd.num2 = view->num2;
    cmd.duration1 = view->duration1;
    cmd.duration2 = view->duration2;
    cmd.int_val = view->flag;
    cmd.flag = view->flag != 0;
    eng->commands.push_back(std::move(cmd));
}

static void engine_bind_keys(EngineState& eng) {
    eng.key_advance = platform_parse_key(eng.config.key_advance);
    eng.key_advance_alt = platform_parse_key(eng.config.key_advance_alt);
    eng.key_quicksave = platform_parse_key(eng.config.key_quicksave);
    eng.key_quickload = platform_parse_key(eng.config.key_quickload);
    eng.key_reload = platform_parse_key(eng.config.key_reload);
    eng.key_exit = platform_parse_key(eng.config.key_exit);
    eng.key_backlog = platform_parse_key(eng.config.key_backlog);
    eng.key_auto_mode = platform_parse_key(eng.config.key_auto_mode);
    eng.key_skip_mode = platform_parse_key(eng.config.key_skip_mode);
    eng.key_hide_ui = platform_parse_key(eng.config.key_hide_ui);
}

static void engine_mount_vfs_and_mods(EngineState& eng) {
    qvn_vfs_init(eng.base_dir.c_str(), QVN_PAK_KEY);
    eng.active_mods.clear();
    QvnModList list = {};
    if (qvn_mods_scan(eng.base_dir.c_str(), &list)) {
        for (size_t i = 0; i < list.count; ++i) {
            if (list.items[i].enabled) eng.active_mods.push_back(list.items[i]);
        }
        qvn_mods_free(&list);
    }
    std::string mods_json = (std::filesystem::path(eng.base_dir) / "mods.json").string();
    eng.mods_fingerprint = qvn_file_fingerprint(mods_json.c_str());
}

static void engine_apply_player_settings(EngineState& eng) {
    eng.config.music_volume = eng.settings.music_volume;
    eng.config.sound_volume = eng.settings.sound_volume;
    eng.config.voice_volume = eng.settings.voice_volume;
    eng.config.typewriter_cps = eng.settings.typewriter_cps;
    eng.config.auto_delay = eng.settings.auto_delay;
    eng.config.typewriter_enabled = eng.settings.typewriter_enabled;
    eng.config.fullscreen = (eng.settings.window_mode == 2) ? 1 : 0;
    eng.config.idle_breathing = eng.settings.idle_breathing;
    audio_apply_config(eng.audio, eng.config);
}

static void engine_refresh_profile(EngineState& eng) {
    eng.gallery_items.clear();
    eng.achievement_items.clear();
    std::string save_dir = platform_resolve_save_dir(eng.config.save_dir);
    QvnProfileItems items = {};
    if (qvn_profile_load(save_dir.c_str(), eng.raw_config_json.c_str(), &items)) {
        for (size_t i = 0; i < items.gallery_count; ++i) {
            GalleryCardCache card;
            card.def = items.gallery[i];
            if (card.def.unlocked && card.def.image[0] != '\0') {
                std::vector<uint8_t> raw;
                if (platform_read_bytes_smart(card.def.image, "image", raw) && !raw.empty()) {
                    card.tex_loaded = card.texture.loadFromMemory(raw.data(), raw.size());
                }
            }
            eng.gallery_items.push_back(std::move(card));
        }
        for (size_t i = 0; i < items.achievement_count; ++i) {
            eng.achievement_items.push_back(items.achievements[i]);
        }
        qvn_profile_free(&items);
    }
}

static void engine_unlock_gallery_auto(EngineState& eng, const std::string& id_or_path) {
    std::string save_dir = platform_resolve_save_dir(eng.config.save_dir);
    for (const auto& g : eng.gallery_items) {
        if (id_or_path == g.def.id || id_or_path == g.def.image) {
            qvn_profile_unlock_gallery(save_dir.c_str(), g.def.id);
            return;
        }
    }
    std::filesystem::path p(id_or_path);
    std::string stem = p.stem().string();
    for (const auto& g : eng.gallery_items) {
        if (stem == g.def.id) {
            qvn_profile_unlock_gallery(save_dir.c_str(), g.def.id);
            return;
        }
    }
}

static bool engine_load_config(EngineState& eng) {
    qvn_config_default(&eng.config);
    eng.raw_config_json.clear();
    std::string resolved = platform_resolve_asset_path(eng.config_path);
    if (platform_read_base_text_file(resolved, eng.raw_config_json)) {
        qvn_config_load_str(eng.raw_config_json.c_str(), &eng.config);
    }

    for (int pass = 0; pass < 2; ++pass) {
        bool want_engine = (pass == 0);
        for (const auto& mod : eng.active_mods) {
            if ((mod.is_engine_mod != 0) != want_engine || mod.config_patch[0] == '\0') continue;
            std::filesystem::path patch_path = std::filesystem::path(mod.root_path) / mod.config_patch;
            std::string patch_raw;
            if (platform_read_base_text_file(patch_path.string(), patch_raw)) {
                qvn_config_apply_json_str(patch_raw.c_str(), &eng.config);
            }
        }
    }

    qvn_saves_set_single_file(eng.config.single_file_saves);
    qvn_settings_from_config(&eng.config, &eng.settings);
    std::string save_dir = platform_resolve_save_dir(eng.config.save_dir);
    qvn_settings_load(save_dir.c_str(), &eng.settings);
    engine_apply_player_settings(eng);
    engine_load_locale(eng);
    engine_refresh_profile(eng);

    eng.config_fingerprint = qvn_file_fingerprint(resolved.c_str());
    engine_bind_keys(eng);

    eng.has_menu_bg = false;
    eng.menu_bg_sprite.reset();
    std::vector<uint8_t> menu_bg_bytes;
    if (eng.config.menu_background[0] != '\0' && platform_read_bytes_smart(eng.config.menu_background, "image", menu_bg_bytes)) {
        if (eng.menu_bg_texture.loadFromMemory(menu_bg_bytes.data(), menu_bg_bytes.size())) {
            eng.menu_bg_sprite.emplace(eng.menu_bg_texture);
            sf::Vector2u sz = eng.menu_bg_texture.getSize();
            if (sz.x > 0 && sz.y > 0) {
                eng.menu_bg_sprite->setScale({LOGICAL_W / static_cast<float>(sz.x), LOGICAL_H / static_cast<float>(sz.y)});
            }
            eng.has_menu_bg = true;
        }
    }
    return true;
}

static void engine_run_mod_hooks(EngineState& eng, bool engine_mods) {
    for (const auto& mod : eng.active_mods) {
        if ((mod.is_engine_mod != 0) != engine_mods || mod.hook_script[0] == '\0') continue;
        std::filesystem::path script_path = std::filesystem::path(mod.root_path) / mod.hook_script;
        std::string code;
        if (platform_read_base_text_file(script_path.string(), code)) {
            qjs_execute_code(eng.script, code.c_str(), code.size(), script_path.string().c_str());
        }
    }
}

static bool engine_execute_entry_script(EngineState& eng) {
    std::string lang = eng.settings.language[0] ? std::string(eng.settings.language) : "en";
    qjs_set_language(eng.script, lang.c_str());
    engine_run_mod_hooks(eng, true);

    std::string resolved = platform_resolve_asset_path(eng.config.entry_script);
    std::string raw_code;
    if (!platform_read_text_file(resolved, raw_code)) {
        std::cerr << "Engine Error: Cannot read script " << resolved << std::endl;
        return false;
    }
    eng.script_fingerprint = qvn_file_fingerprint(resolved.c_str());

    if (ends_with_ci(resolved, ".rpy")) {
        char* transpiled = qvn_transpile_rpy(raw_code.c_str());
        if (!transpiled) return false;
        int ok = qjs_execute_code(eng.script, transpiled, std::char_traits<char>::length(transpiled), resolved.c_str());
        qvn_free_cstr(transpiled);
        if (!ok) return false;
    } else {
        if (!qjs_execute_code(eng.script, raw_code.c_str(), raw_code.size(), resolved.c_str())) return false;
    }

    engine_run_mod_hooks(eng, false);
    return true;
}

static void engine_refresh_save_slots(EngineState& eng, bool for_saving) {
    eng.slot_metas.clear();
    eng.slot_thumbs.clear();
    std::string save_dir = platform_resolve_save_dir(eng.config.save_dir);
    auto push_slot = [&](const std::string& slot_name) {
        QvnSaveSlotMeta meta = {};
        qvn_get_save_slot_meta(save_dir.c_str(), slot_name.c_str(), &meta);
        eng.slot_metas.push_back(meta);
        std::optional<sf::Texture> tex;
        if (meta.exists) {
            std::filesystem::path tp = std::filesystem::path(save_dir) / (std::string(meta.slot) + ".png");
            std::error_code ec;
            if (std::filesystem::is_regular_file(tp, ec)) {
                sf::Texture t;
                if (t.loadFromFile(tp.string())) {
                    tex.emplace(std::move(t));
                }
            }
        }
        eng.slot_thumbs.push_back(std::move(tex));
    };
    if (!for_saving) {
        push_slot("autosave");
        push_slot("quicksave");
    }
    int total_slots = std::clamp(static_cast<int>(eng.config.save_slots_count), 6, 60);
    for (int i = 1; i <= total_slots; ++i) {
        push_slot("slot" + std::to_string(i));
    }
}

static void engine_execute_next_command(EngineState& eng);
static bool engine_save_slot(EngineState& eng, const std::string& slot, const std::string& toast_msg);
static bool engine_load_slot(EngineState& eng, const std::string& slot);
static bool engine_load_latest(EngineState& eng);
static void engine_pick_choice(EngineState& eng, int selected);
static void engine_handle_advance(EngineState& eng);
static void engine_apply_window_settings(EngineState& eng);
static void engine_hot_reload(EngineState& eng, bool reload_config);

static bool engine_start_new_game(EngineState& eng) {
    eng.commands.clear();
    eng.current_command_idx = 0;
    eng.choice_history.clear();
    eng.backlog.clear();
    eng.waiting_for_click = false;
    eng.waiting_for_choice = false;
    eng.current_choice_callback_id = -1;
    eng.auto_mode = false;
    eng.skip_mode = false;
    eng.ui_hidden = false;
    eng.wait_timer = 0.f;
    eng.ending_title.clear();
    eng.ending_subtitle.clear();

    if (!qjs_reset(eng.script)) return false;
    if (!engine_execute_entry_script(eng)) return false;

    scene_reset_visuals(eng.scene, eng.config);
    eng.screen = GameScreen::Playing;
    engine_execute_next_command(eng);
    return true;
}

static bool engine_rebuild_state(
    EngineState& eng,
    size_t target_cmd_idx,
    const std::vector<int32_t>& choices,
    const std::string& js_state_json
) {
    eng.commands.clear();
    eng.current_command_idx = 0;
    eng.choice_history.clear();
    eng.backlog.clear();
    eng.waiting_for_click = false;
    eng.waiting_for_choice = false;
    eng.current_choice_callback_id = -1;
    eng.wait_timer = 0.f;

    if (!qjs_reset(eng.script)) return false;
    if (!engine_execute_entry_script(eng)) return false;

    scene_reset_visuals(eng.scene, eng.config);

    std::string pending_bg;
    bool has_pending_bg = false;

    struct PendingChar {
        std::string id;
        std::string path;
        float x;
        float y;
        bool visible;
    };
    std::vector<PendingChar> pending_chars;

    std::string pending_music;
    bool pending_music_loop = true;
    bool has_pending_music = false;
    bool stop_music_requested = false;

    std::string last_speaker;
    std::string last_text;
    std::string last_color;
    std::string last_char_id;
    float last_cps = -1.f;
    bool has_last_dialogue = false;
    bool landed_on_say = false;
    bool pending_nvl = false;

    size_t choice_cursor = 0;

    while (eng.current_command_idx < target_cmd_idx && eng.current_command_idx < eng.commands.size()) {
        Command cmd = eng.commands[eng.current_command_idx++];
        landed_on_say = false;

        if (cmd.type == CommandType::ShowBackground) {
            pending_bg = cmd.arg1;
            has_pending_bg = true;
        } else if (cmd.type == CommandType::ShowCharacter) {
            std::string cid = cmd.arg2.empty() ? "default" : cmd.arg2;
            PendingChar* found = nullptr;
            for (auto& pc : pending_chars) {
                if (pc.id == cid) { found = &pc; break; }
            }
            if (!found) {
                pending_chars.push_back({cid, cmd.arg1, eng.config.default_char_x, eng.config.default_char_y, true});
                found = &pending_chars.back();
            }
            found->path = cmd.arg1;
            found->visible = true;
            if (!std::isnan(cmd.num1)) found->x = cmd.num1;
            if (!std::isnan(cmd.num2)) found->y = cmd.num2;
        } else if (cmd.type == CommandType::MoveCharacter) {
            std::string cid = cmd.arg2.empty() ? "default" : cmd.arg2;
            for (auto& pc : pending_chars) {
                if (pc.id == cid) {
                    if (!std::isnan(cmd.num1)) pc.x = cmd.num1;
                    if (!std::isnan(cmd.num2)) pc.y = cmd.num2;
                }
            }
        } else if (cmd.type == CommandType::HideCharacter) {
            std::string cid = cmd.arg2.empty() ? "default" : cmd.arg2;
            for (auto& pc : pending_chars) {
                if (pc.id == cid) pc.visible = false;
            }
        } else if (cmd.type == CommandType::HideAllCharacters) {
            for (auto& pc : pending_chars) pc.visible = false;
        } else if (cmd.type == CommandType::PlayMusic) {
            pending_music = cmd.arg1;
            pending_music_loop = cmd.flag;
            has_pending_music = true;
            stop_music_requested = false;
        } else if (cmd.type == CommandType::StopMusic) {
            has_pending_music = false;
            stop_music_requested = true;
            pending_music.clear();
        } else if (cmd.type == CommandType::Weather) {
            scene_set_weather(eng.scene, cmd.arg1, cmd.num1);
        } else if (cmd.type == CommandType::Cinema) {
            scene_set_cinema(eng.scene, cmd.flag);
        } else if (cmd.type == CommandType::Vignette) {
            scene_set_vignette(eng.scene, cmd.num1);
        } else if (cmd.type == CommandType::NvlMode) {
            pending_nvl = cmd.flag;
        } else if (cmd.type == CommandType::NvlClear) {
            scene_clear_nvl(eng.scene);
        } else if (cmd.type == CommandType::Say) {
            last_speaker = cmd.arg1;
            last_text = cmd.arg2;
            last_color = cmd.arg3;
            last_char_id = cmd.arg4;
            last_cps = cmd.num1;
            has_last_dialogue = true;
            landed_on_say = true;
            eng.backlog.push_back({cmd.arg1, cmd.arg2});
        } else if (cmd.type == CommandType::ShowChoices || cmd.type == CommandType::TimedChoice) {
            if (choice_cursor < choices.size()) {
                int32_t picked = choices[choice_cursor++];
                eng.choice_history.push_back(picked);
                qjs_invoke_callback(eng.script, cmd.callback_id, picked);
            } else {
                scene_set_choices(eng.scene, eng.config, cmd.choices, cmd.type == CommandType::TimedChoice ? cmd.duration1 : 0.f);
                eng.current_choice_callback_id = cmd.callback_id;
                eng.timed_choice_default_idx = cmd.int_val;
                eng.waiting_for_choice = true;
                break;
            }
        }
    }

    scene_set_nvl_mode(eng.scene, pending_nvl);
    if (has_pending_bg) scene_set_background(eng.scene, eng.config, pending_bg, 0.f);
    for (const auto& pc : pending_chars) {
        if (pc.visible) scene_set_character(eng.scene, eng.config, pc.id, pc.path, pc.x, pc.y, 0.f, 0.f);
    }
    if (stop_music_requested) audio_stop_music(eng.audio, 0.f);
    else if (has_pending_music) audio_play_music(eng.audio, pending_music, pending_music_loop, 0.f);

    if (!js_state_json.empty()) qjs_deserialize_state(eng.script, js_state_json.c_str());

    if (has_last_dialogue) {
        scene_set_dialogue(eng.scene, eng.config, last_speaker, last_text, last_cps, last_color, last_char_id);
        scene_skip_typewriter(eng.scene);
    }

    if (!eng.waiting_for_choice) {
        if (landed_on_say) eng.waiting_for_click = true;
        else engine_execute_next_command(eng);
    }
    return true;
}

static bool engine_save_slot(EngineState& eng, const std::string& slot, const std::string& toast_msg) {
    std::string target_slot = slot.empty() ? std::string(eng.config.default_save_slot) : slot;
    std::string save_dir = platform_resolve_save_dir(eng.config.save_dir);
    char* state_cstr = qjs_serialize_state(eng.script);
    const char* state_ptr = state_cstr ? state_cstr : "{}";

    int ok = qvn_save_state_ex(
        save_dir.c_str(),
        target_slot.c_str(),
        static_cast<uint64_t>(eng.current_command_idx),
        eng.choice_history.empty() ? nullptr : eng.choice_history.data(),
        eng.choice_history.size(),
        state_ptr,
        eng.scene.speaker_name.c_str(),
        eng.scene.raw_dialogue.c_str()
    );

    if (state_cstr) qjs_free_cstr(state_cstr);
    if (ok) {
        if (eng.screen == GameScreen::Playing) {
            engine_capture_scene_thumb(eng);
        }
        if (eng.has_scene_thumb) {
            std::filesystem::path tp = std::filesystem::path(save_dir) / (target_slot + ".png");
            (void)eng.last_scene_thumb.saveToFile(tp.string());
        }
        if (eng.config.ui_sound_save[0] != '\0') audio_play_sound(eng.audio, eng.config.ui_sound_save);
        scene_show_toast(eng.scene, toast_msg.empty() ? ("Saved: " + target_slot) : toast_msg);
    }
    return ok != 0;
}

static bool engine_load_slot(EngineState& eng, const std::string& slot) {
    std::string target_slot = slot.empty() ? std::string(eng.config.default_save_slot) : slot;
    std::string save_dir = platform_resolve_save_dir(eng.config.save_dir);

    QvnSaveData data = {};
    if (!qvn_load_state_json(save_dir.c_str(), target_slot.c_str(), &data)) {
        scene_show_toast(eng.scene, L(eng, "no_saves", "No saves found"));
        return false;
    }

    size_t target_idx = static_cast<size_t>(data.command_idx);
    std::vector<int32_t> saved_choices;
    if (data.choice_history && data.choice_count > 0) {
        saved_choices.assign(data.choice_history, data.choice_history + data.choice_count);
    }
    std::string saved_js_state = data.js_state_json ? data.js_state_json : "{}";
    qvn_free_save_data(&data);

    eng.screen = GameScreen::Playing;
    bool ok = engine_rebuild_state(eng, target_idx, saved_choices, saved_js_state);
    if (ok) {
        if (eng.config.ui_sound_load[0] != '\0') audio_play_sound(eng.audio, eng.config.ui_sound_load);
        scene_show_toast(eng.scene, L(eng, "loaded", "Loaded") + " (" + target_slot + ")");
    }
    return ok;
}

static bool engine_load_latest(EngineState& eng) {
    std::string save_dir = platform_resolve_save_dir(eng.config.save_dir);
    char latest[64] = {0};
    if (!qvn_find_latest_save_slot(save_dir.c_str(), latest, sizeof(latest))) {
        scene_show_toast(eng.scene, L(eng, "no_saves", "No saves found"));
        return false;
    }
    return engine_load_slot(eng, latest);
}

static void engine_apply_window_settings(EngineState& eng) {
    sf::VideoMode desktop = sf::VideoMode::getDesktopMode();
    uint32_t req_w = eng.settings.res_width >= 640 ? eng.settings.res_width : eng.config.window_width;
    uint32_t req_h = eng.settings.res_height >= 360 ? eng.settings.res_height : eng.config.window_height;

    uint32_t w = req_w;
    uint32_t h = req_h;
    int pos_x = 0;
    int pos_y = 0;

    if (eng.settings.window_mode == 2 || eng.settings.window_mode == 1) {
        w = desktop.size.x;
        h = desktop.size.y;
        pos_x = 0;
        pos_y = 0;
    } else {
        if (w > desktop.size.x || h > desktop.size.y) {
            float sx = static_cast<float>(desktop.size.x) / static_cast<float>(w);
            float sy = static_cast<float>(desktop.size.y) / static_cast<float>(h);
            float k = std::min(sx, sy);
            w = std::max<uint32_t>(640, static_cast<uint32_t>(std::floor(static_cast<float>(w) * k)));
            h = std::max<uint32_t>(360, static_cast<uint32_t>(std::floor(static_cast<float>(h) * k)));
        }
        pos_x = desktop.size.x > w ? static_cast<int>((desktop.size.x - w) / 2) : 0;
        pos_y = desktop.size.y > h ? static_cast<int>((desktop.size.y - h) / 2) : 0;
    }

    if (eng.window.isOpen() && eng.created_window_mode == eng.settings.window_mode && eng.window.getSize().x == w && eng.window.getSize().y == h) {
        eng.window.setView(platform_compute_view(eng.window.getSize(), LOGICAL_W, LOGICAL_H, eng.settings.scale_mode));
        return;
    }

    sf::State win_state = (eng.settings.window_mode == 2) ? sf::State::Fullscreen : sf::State::Windowed;
    uint32_t style = (eng.settings.window_mode == 1) ? sf::Style::None : sf::Style::Default;

    sf::ContextSettings ctx_settings;
    ctx_settings.antiAliasingLevel = 4;
    eng.window.create(
        sf::VideoMode({w, h}),
        sf::String::fromUtf8(eng.config.window_title, eng.config.window_title + std::char_traits<char>::length(eng.config.window_title)),
        style,
        win_state,
        ctx_settings
    );
    if (eng.settings.window_mode != 2) {
        eng.window.setPosition({pos_x, pos_y});
    }
    eng.created_window_mode = eng.settings.window_mode;
    eng.window.setVerticalSyncEnabled(eng.config.vsync != 0);
    if (eng.config.fps_limit > 0) eng.window.setFramerateLimit(eng.config.fps_limit);
    eng.window.setView(platform_compute_view(eng.window.getSize(), LOGICAL_W, LOGICAL_H, eng.settings.scale_mode));
}

static void engine_hot_reload(EngineState& eng, bool reload_config) {
    engine_mount_vfs_and_mods(eng);
    if (reload_config) {
        engine_load_config(eng);
        std::string lang = eng.settings.language[0] ? std::string(eng.settings.language) : "en";
        scene_apply_config(eng.scene, eng.config, lang);
        audio_apply_config(eng.audio, eng.config);
        eng.window.setView(platform_compute_view(eng.window.getSize(), LOGICAL_W, LOGICAL_H, eng.settings.scale_mode));
    }
    bool in_session = (eng.screen == GameScreen::Playing) || (eng.return_screen == GameScreen::PauseMenu && !eng.commands.empty());
    if (!in_session) return;

    char* state_cstr = qjs_serialize_state(eng.script);
    std::string preserved_js = state_cstr ? state_cstr : "{}";
    if (state_cstr) qjs_free_cstr(state_cstr);

    engine_rebuild_state(eng, eng.current_command_idx, eng.choice_history, preserved_js);
    scene_show_toast(eng.scene, "Hot-Reloaded");
}

static std::string engine_interpolate_text(EngineState& eng, const std::string& input) {
    if (!eng.script || input.find('{') == std::string::npos) return input;
    std::string out;
    out.reserve(input.size());
    size_t i = 0;
    while (i < input.size()) {
        if (input[i] == '{') {
            size_t close = input.find('}', i + 1);
            if (close != std::string::npos && close > i + 1 && close - i < 64) {
                std::string key = input.substr(i + 1, close - i - 1);
                char* val = qjs_get_state_var(eng.script, key.c_str());
                if (val) {
                    out += val;
                    qjs_free_cstr(val);
                    i = close + 1;
                    continue;
                }
            }
        }
        out.push_back(input[i++]);
    }
    return out;
}

static void engine_execute_next_command(EngineState& eng) {
    if (eng.current_command_idx >= eng.commands.size()) {
        eng.waiting_for_click = false;
        if (!eng.waiting_for_choice && eng.wait_timer <= 0.f && eng.screen == GameScreen::Playing) {
            eng.ending_title = L(eng, "the_end", "The End");
            eng.ending_subtitle = L(eng, "ending_sub", "Thank you for playing");
            eng.screen = GameScreen::EndScreen;
        }
        return;
    }

    eng.waiting_for_click = false;

    while (eng.current_command_idx < eng.commands.size() && !eng.waiting_for_click && !eng.waiting_for_choice && eng.wait_timer <= 0.f && eng.screen == GameScreen::Playing) {
        Command cmd = eng.commands[eng.current_command_idx++];

        if (cmd.type == CommandType::ShowBackground) {
            scene_set_background(eng.scene, eng.config, cmd.arg1, cmd.duration1);
            engine_unlock_gallery_auto(eng, cmd.arg1);
        } else if (cmd.type == CommandType::ShowCharacter) {
            scene_set_character(eng.scene, eng.config, cmd.arg2, cmd.arg1, cmd.num1, cmd.num2, cmd.duration1, cmd.duration2);
        } else if (cmd.type == CommandType::MoveCharacter) {
            scene_move_character(eng.scene, eng.config, cmd.arg2, cmd.num1, cmd.num2, cmd.duration1);
        } else if (cmd.type == CommandType::HideCharacter) {
            scene_hide_character(eng.scene, eng.config, cmd.arg2, cmd.duration1);
        } else if (cmd.type == CommandType::HideAllCharacters) {
            scene_hide_all_characters(eng.scene, eng.config, cmd.duration1);
        } else if (cmd.type == CommandType::ScaleCharacter) {
            scene_scale_character(eng.scene, cmd.arg2, cmd.num1, cmd.duration1);
        } else if (cmd.type == CommandType::BounceCharacter) {
            scene_bounce_character(eng.scene, cmd.arg2);
        } else if (cmd.type == CommandType::FlipCharacter) {
            scene_flip_character(eng.scene, cmd.arg2);
        } else if (cmd.type == CommandType::CamZoom) {
            scene_trigger_cam_zoom(eng.scene, cmd.num1, cmd.duration1);
        } else if (cmd.type == CommandType::CamPan) {
            scene_trigger_cam_pan(eng.scene, cmd.num1, cmd.num2, cmd.duration1);
        } else if (cmd.type == CommandType::Weather) {
            scene_set_weather(eng.scene, cmd.arg1, cmd.num1);
        } else if (cmd.type == CommandType::Cinema) {
            scene_set_cinema(eng.scene, cmd.flag);
        } else if (cmd.type == CommandType::Vignette) {
            scene_set_vignette(eng.scene, cmd.num1);
        } else if (cmd.type == CommandType::PlayMusic) {
            audio_play_music(eng.audio, cmd.arg1, cmd.flag, cmd.duration1 < 0.f ? 0.35f : cmd.duration1);
        } else if (cmd.type == CommandType::StopMusic) {
            audio_stop_music(eng.audio, cmd.duration1 < 0.f ? 0.35f : cmd.duration1);
        } else if (cmd.type == CommandType::PlaySound) {
            audio_play_sound(eng.audio, cmd.arg1);
        } else if (cmd.type == CommandType::PlayVoice) {
            audio_play_voice(eng.audio, cmd.arg1);
        } else if (cmd.type == CommandType::Shake) {
            scene_trigger_shake(eng.scene, cmd.num1, cmd.duration1);
        } else if (cmd.type == CommandType::Flash) {
            scene_trigger_flash(eng.scene, cmd.arg1, cmd.duration1);
        } else if (cmd.type == CommandType::Tint) {
            scene_trigger_tint(eng.scene, cmd.arg1, cmd.duration1);
        } else if (cmd.type == CommandType::Wait) {
            eng.wait_timer = cmd.duration1 > 0.f ? cmd.duration1 : 0.5f;
            return;
        } else if (cmd.type == CommandType::Chapter) {
            float dur = cmd.duration1 > 0.f ? cmd.duration1 : 2.5f;
            scene_show_chapter(eng.scene, cmd.arg1, cmd.arg2, dur);
            eng.wait_timer = dur;
            return;
        } else if (cmd.type == CommandType::Notify) {
            scene_show_toast(eng.scene, cmd.arg1);
        } else if (cmd.type == CommandType::PromptInput) {
            eng.prompt_text = cmd.arg1;
            eng.prompt_state_key = cmd.arg2;
            eng.prompt_buffer = cmd.arg3;
            eng.ui.focused_input_id = 555;
            eng.screen = GameScreen::PromptModal;
            return;
        } else if (cmd.type == CommandType::NvlMode) {
            scene_set_nvl_mode(eng.scene, cmd.flag);
        } else if (cmd.type == CommandType::NvlClear) {
            scene_clear_nvl(eng.scene);
        } else if (cmd.type == CommandType::UnlockGallery) {
            std::string save_dir = platform_resolve_save_dir(eng.config.save_dir);
            if (qvn_profile_unlock_gallery(save_dir.c_str(), cmd.arg1.c_str())) {
                scene_show_toast(eng.scene, "CG Unlocked: " + cmd.arg1);
            }
        } else if (cmd.type == CommandType::UnlockAchievement) {
            std::string save_dir = platform_resolve_save_dir(eng.config.save_dir);
            if (qvn_profile_unlock_achievement(save_dir.c_str(), cmd.arg1.c_str())) {
                engine_refresh_profile(eng);
                std::string tkey = "ach_" + cmd.arg1 + "_title";
                std::string fallback_title = cmd.arg1;
                for (const auto& a : eng.achievement_items) {
                    if (cmd.arg1 == a.id) { fallback_title = a.title; break; }
                }
                if (eng.config.ui_sound_achievement[0] != '\0') audio_play_sound(eng.audio, eng.config.ui_sound_achievement);
                scene_show_achievement_toast(eng.scene, L(eng, tkey.c_str(), fallback_title.c_str()));
            }
        } else if (cmd.type == CommandType::Ending) {
            eng.ending_title = cmd.arg1.empty() ? L(eng, "the_end", "The End") : cmd.arg1;
            eng.ending_subtitle = cmd.arg2.empty() ? L(eng, "ending_sub", "Thank you for playing") : cmd.arg2;
            eng.screen = GameScreen::EndScreen;
            return;
        } else if (cmd.type == CommandType::Say) {
            std::string spk = engine_interpolate_text(eng, cmd.arg1);
            std::string txt = engine_interpolate_text(eng, cmd.arg2);
            scene_set_dialogue(eng.scene, eng.config, spk, txt, cmd.num1, cmd.arg3, cmd.arg4);
            eng.backlog.push_back({spk, txt});
            size_t max_log = eng.config.backlog_limit > 10 ? eng.config.backlog_limit : 200;
            while (eng.backlog.size() > max_log) eng.backlog.erase(eng.backlog.begin());
            eng.waiting_for_click = true;
            eng.auto_timer = 0.f;
        } else if (cmd.type == CommandType::ShowChoices || cmd.type == CommandType::TimedChoice) {
            scene_set_choices(eng.scene, eng.config, cmd.choices, cmd.type == CommandType::TimedChoice ? cmd.duration1 : 0.f);
            eng.current_choice_callback_id = cmd.callback_id;
            eng.timed_choice_default_idx = cmd.int_val;
            eng.waiting_for_choice = true;
            eng.skip_mode = false;
            eng.auto_mode = false;
        } else if (cmd.type == CommandType::SaveState) {
            engine_save_slot(eng, cmd.arg1, "");
        } else if (cmd.type == CommandType::Autosave) {
            engine_save_slot(eng, "autosave", L(eng, "autosaved", "Autosaved"));
        } else if (cmd.type == CommandType::LoadState) {
            engine_load_slot(eng, cmd.arg1);
            return;
        }
    }

    if (eng.screen == GameScreen::Playing && eng.current_command_idx >= eng.commands.size() && !eng.waiting_for_click && !eng.waiting_for_choice && eng.wait_timer <= 0.f) {
        eng.ending_title = L(eng, "the_end", "The End");
        eng.ending_subtitle = L(eng, "ending_sub", "Thank you for playing");
        eng.screen = GameScreen::EndScreen;
    }
}

static void engine_pick_choice(EngineState& eng, int selected) {
    if (!eng.waiting_for_choice || selected < 0) return;
    if (eng.config.ui_sound_choice[0] != '\0') audio_play_sound(eng.audio, eng.config.ui_sound_choice);
    eng.waiting_for_choice = false;
    scene_clear_choices(eng.scene);
    int cb_id = eng.current_choice_callback_id;
    eng.current_choice_callback_id = -1;
    eng.choice_history.push_back(selected);
    qjs_invoke_callback(eng.script, cb_id, selected);
    engine_execute_next_command(eng);
}

static void engine_handle_advance(EngineState& eng) {
    if (eng.ui_hidden) {
        eng.ui_hidden = false;
        return;
    }
    if (eng.waiting_for_choice) return;
    if (scene_is_typing(eng.scene)) {
        scene_skip_typewriter(eng.scene);
        return;
    }
    if (eng.waiting_for_click) {
        engine_execute_next_command(eng);
    }
}

static sf::FloatRect engine_hud_bar_rect(const EngineState& eng) {
    int count = (eng.config.hud_show_backlog ? 1 : 0)
        + (eng.config.hud_show_auto ? 1 : 0)
        + (eng.config.hud_show_skip ? 1 : 0)
        + (eng.config.hud_show_save ? 1 : 0)
        + (eng.config.hud_show_load ? 1 : 0)
        + (eng.config.hud_show_hide ? 1 : 0)
        + (eng.config.hud_show_menu ? 1 : 0);
    float w = std::max(0, count) * 56.f + 16.f;
    std::string pos = eng.config.hud_position;
    if (pos == "bottom_center") {
        return sf::FloatRect({(LOGICAL_W - w) * 0.5f, 678.f}, {w, 30.f});
    } else if (pos == "top_right") {
        return sf::FloatRect({LOGICAL_W - w - 24.f, 20.f}, {w, 30.f});
    }
    return sf::FloatRect({eng.config.box_x + eng.config.box_width - w, 678.f}, {w, 30.f});
}

static void engine_render_hud(EngineState& eng) {
    if (!eng.config.hud_enabled || eng.ui_hidden || eng.screen != GameScreen::Playing) return;
    if (!eng.waiting_for_click && !eng.waiting_for_choice) return;

    sf::FloatRect bar = engine_hud_bar_rect(eng);
    if (bar.size.x <= 20.f) return;

    ui_draw_glass_panel(eng.window, bar, 15.f, sf::Color(10, 11, 16, 210), sf::Color(255, 255, 255, 26), 45);

    float x = bar.position.x + bar.size.x - 8.f;
    float y = bar.position.y + 4.f;
    float btn_w = 52.f;
    float btn_h = 22.f;
    float gap = 4.f;

    sf::Color idle_bg(0, 0, 0, 0);
    sf::Color hov_bg(255, 255, 255, 24);
    sf::Color active_bg = to_sf_color(eng.config.theme_accent_color);
    sf::Color no_br(0, 0, 0, 0);

    auto hud_btn = [&](bool show, const std::string& label, bool active) -> bool {
        if (!show) return false;
        x -= btn_w;
        bool clicked = ui_button(eng.ui, eng.window, sf::FloatRect({x, y}, {btn_w, btn_h}), label, active ? active_bg : idle_bg, active ? active_bg : hov_bg, no_br, 12, 11.f);
        x -= gap;
        return clicked;
    };

    if (hud_btn(eng.config.hud_show_menu, L(eng, "hud_menu", "Menu"), false)) {
        engine_capture_scene_thumb(eng);
        eng.screen = GameScreen::PauseMenu;
    }
    if (hud_btn(eng.config.hud_show_hide, L(eng, "hud_hide", "Hide"), false)) {
        eng.ui_hidden = true;
    }
    if (hud_btn(eng.config.hud_show_load, L(eng, "hud_load", "Load"), false)) {
        engine_load_latest(eng);
    }
    if (hud_btn(eng.config.hud_show_save, L(eng, "hud_save", "Save"), false)) {
        engine_save_slot(eng, "quicksave", L(eng, "quicksaved", "Quicksaved"));
    }
    if (hud_btn(eng.config.hud_show_skip, L(eng, "hud_skip", "Skip"), eng.skip_mode)) {
        eng.skip_mode = !eng.skip_mode;
        if (eng.skip_mode) eng.auto_mode = false;
    }
    if (hud_btn(eng.config.hud_show_auto, L(eng, "hud_auto", "Auto"), eng.auto_mode)) {
        eng.auto_mode = !eng.auto_mode;
        if (eng.auto_mode) eng.skip_mode = false;
    }
    if (hud_btn(eng.config.hud_show_backlog, L(eng, "hud_log", "Log"), false)) {
        eng.backlog_scroll = std::max(0, static_cast<int>(eng.backlog.size()) - 8);
        eng.return_screen = GameScreen::Playing;
        eng.screen = GameScreen::BacklogMenu;
    }
}

static void engine_render_main_menu(EngineState& eng, bool interactive = true) {
    if (eng.has_menu_bg && eng.menu_bg_sprite) {
        eng.window.draw(*eng.menu_bg_sprite);
    } else {
        ui_draw_gradient_rect(eng.window, sf::FloatRect({0.f, 0.f}, {LOGICAL_W, LOGICAL_H}), sf::Color(9, 10, 15), sf::Color(24, 24, 38));
    }

    std::string align = eng.config.menu_align;
    QvnColor ov = eng.config.menu_overlay_color;
    if (align == "right") {
        ui_draw_horizontal_gradient(eng.window, sf::FloatRect({0.f, 0.f}, {LOGICAL_W, LOGICAL_H}), sf::Color(ov.r, ov.g, ov.b, 35), sf::Color(ov.r, ov.g, ov.b, ov.a));
    } else if (align == "center") {
        ui_draw_gradient_rect(eng.window, sf::FloatRect({0.f, 0.f}, {LOGICAL_W, LOGICAL_H}), sf::Color(ov.r, ov.g, ov.b, 140), sf::Color(ov.r, ov.g, ov.b, ov.a));
    } else {
        ui_draw_horizontal_gradient(eng.window, sf::FloatRect({0.f, 0.f}, {LOGICAL_W, LOGICAL_H}), sf::Color(ov.r, ov.g, ov.b, ov.a), sf::Color(ov.r, ov.g, ov.b, 35));
    }

    float col_w = 320.f;
    float col_x = 92.f;
    if (align == "center") col_x = (LOGICAL_W - col_w) * 0.5f;
    else if (align == "right") col_x = LOGICAL_W - col_w - 92.f;

    sf::Color acc = to_sf_color(eng.config.theme_accent_color);

    ui_draw_label(eng.ui, eng.window, eng.config.menu_title, {col_x + 2.f, 96.f}, 54, sf::Color(0, 0, 0, 140));
    ui_draw_label(eng.ui, eng.window, eng.config.menu_title, {col_x, 94.f}, 54, to_sf_color(eng.config.menu_title_color));

    if (eng.config.menu_subtitle[0] != '\0') {
        ui_draw_label(eng.ui, eng.window, eng.config.menu_subtitle, {col_x + 2.f, 160.f}, 16, to_sf_color(eng.config.menu_subtitle_color));
    }

    sf::FloatRect div_line({col_x, 194.f}, {56.f, 3.f});
    ui_draw_rounded_panel(eng.window, div_line, 1.5f, acc, sf::Color::Transparent, 0.f, 0);

    auto menu_item = [&](int idx, float y, const std::string& label, bool primary) -> bool {
        sf::FloatRect hit({col_x, y}, {col_w, 42.f});
        bool hovered = interactive && hit.contains(eng.ui.mouse_pos);
        float anim = ui_anim_hover(eng.ui, 70000ULL + static_cast<uint64_t>(idx), hovered);

        sf::FloatRect r = hit;
        r.position.x += anim * 10.f;

        sf::Color bg = primary
            ? sf::Color(acc.r, acc.g, acc.b, static_cast<uint8_t>(48.f + anim * 58.f))
            : sf::Color(255, 255, 255, static_cast<uint8_t>(anim * 22.f));
        sf::Color br = primary
            ? sf::Color(acc.r, acc.g, acc.b, static_cast<uint8_t>(115.f + anim * 100.f))
            : sf::Color(255, 255, 255, static_cast<uint8_t>(anim * 35.f));

        if (bg.a > 2) ui_draw_rounded_panel(eng.window, r, 10.f, bg, br, 1.f, 0);

        if (anim > 0.05f || primary) {
            float bar_a = primary ? 255.f : anim * 255.f;
            sf::Color bar_col(acc.r, acc.g, acc.b, static_cast<uint8_t>(bar_a));
            ui_draw_rounded_panel(eng.window, sf::FloatRect({r.position.x + 8.f, r.position.y + 9.f}, {3.f, 24.f}), 1.5f, bar_col, sf::Color::Transparent, 0.f, 0);
        }

        sf::Color fg = (primary || hovered) ? sf::Color(250, 250, 250) : sf::Color(212, 212, 216);
        ui_draw_label(eng.ui, eng.window, label, {r.position.x + 24.f, r.position.y + 9.f}, 17, fg);
        if (interactive && hovered && eng.ui.mouse_clicked) {
            ui_consume_click(eng.ui);
            return true;
        }
        return false;
    };

    float by = 224.f;
    float step = 48.f;
    int slot = 0;

    if (menu_item(slot, by + static_cast<float>(slot) * step, L(eng, "new_game", "New Game"), true)) {
        engine_start_new_game(eng);
    }
    slot++;
    if (eng.config.menu_show_continue) {
        if (menu_item(slot, by + static_cast<float>(slot) * step, L(eng, "continue", "Continue"), false)) {
            engine_load_latest(eng);
        }
        slot++;
    }
    if (eng.config.menu_show_load) {
        if (menu_item(slot, by + static_cast<float>(slot) * step, L(eng, "load_game", "Load Game"), false)) {
            engine_refresh_save_slots(eng, false);
            eng.return_screen = GameScreen::MainMenu;
            eng.screen = GameScreen::LoadMenu;
        }
        slot++;
    }
    if (eng.config.gallery_enabled && eng.config.menu_show_gallery) {
        if (menu_item(slot, by + static_cast<float>(slot) * step, L(eng, "gallery", "Gallery"), false)) {
            engine_refresh_profile(eng);
            eng.gallery_lightbox_idx = -1;
            eng.return_screen = GameScreen::MainMenu;
            eng.screen = GameScreen::GalleryMenu;
        }
        slot++;
    }
    if (eng.config.achievements_enabled && eng.config.menu_show_achievements) {
        if (menu_item(slot, by + static_cast<float>(slot) * step, L(eng, "achievements", "Achievements"), false)) {
            engine_refresh_profile(eng);
            eng.return_screen = GameScreen::MainMenu;
            eng.screen = GameScreen::AchievementsMenu;
        }
        slot++;
    }
    if (eng.config.mods_enabled && eng.config.menu_show_mods) {
        if (menu_item(slot, by + static_cast<float>(slot) * step, L(eng, "mods", "Mods"), false)) {
            mod_menu_reload(eng.mod_menu, eng.base_dir);
            eng.return_screen = GameScreen::MainMenu;
            eng.screen = GameScreen::ModsMenu;
        }
        slot++;
    }
    if (eng.config.menu_show_settings) {
        if (menu_item(slot, by + static_cast<float>(slot) * step, L(eng, "settings", "Settings"), false)) {
            eng.return_screen = GameScreen::MainMenu;
            eng.screen = GameScreen::SettingsMenu;
        }
        slot++;
    }
    if (eng.config.menu_show_quit) {
        if (menu_item(slot, by + static_cast<float>(slot) * step, L(eng, "quit", "Exit"), false)) {
            eng.window.close();
        }
    }
}

static void engine_render_pause_menu(EngineState& eng) {
    sf::RectangleShape dim({LOGICAL_W, LOGICAL_H});
    dim.setFillColor(sf::Color(5, 6, 10, 175));
    eng.window.draw(dim);

    float pw = 340.f;
    float ph = eng.config.mods_enabled ? 534.f : 486.f;
    float px = (LOGICAL_W - pw) * 0.5f;
    float py = (LOGICAL_H - ph) * 0.5f;

    ui_draw_glass_panel(eng.window, sf::FloatRect({px, py}, {pw, ph}), 18.f, to_sf_color(eng.config.theme_panel_color), to_sf_color(eng.config.theme_panel_border_color), 115);
    ui_draw_label_centered(eng.ui, eng.window, L(eng, "paused", "Game Paused"), sf::FloatRect({px, py + 16.f}, {pw, 32.f}), 21, sf::Color(250, 250, 250));

    sf::Color btn_bg = to_sf_color(eng.config.theme_button_color);
    sf::Color btn_hov = to_sf_color(eng.config.theme_button_hover_color);
    sf::Color btn_br = to_sf_color(eng.config.theme_panel_border_color);
    sf::Color acc = to_sf_color(eng.config.theme_accent_color);

    float bx = px + 26.f;
    float bw = pw - 52.f;
    float by = py + 64.f;
    float step = 48.f;
    int idx = 0;

    if (ui_button(eng.ui, eng.window, sf::FloatRect({bx, by + step * idx++}, {bw, 40.f}), L(eng, "resume", "Resume"), acc, to_sf_color(eng.config.theme_accent_hover_color), acc, 15, 10.f, UiIcon::Play)) {
        eng.screen = GameScreen::Playing;
    }
    if (ui_button(eng.ui, eng.window, sf::FloatRect({bx, by + step * idx++}, {bw, 40.f}), L(eng, "save_game", "Save Game"), btn_bg, btn_hov, btn_br, 15, 10.f)) {
        engine_refresh_save_slots(eng, true);
        eng.return_screen = GameScreen::PauseMenu;
        eng.screen = GameScreen::SaveMenu;
    }
    if (ui_button(eng.ui, eng.window, sf::FloatRect({bx, by + step * idx++}, {bw, 40.f}), L(eng, "load_game", "Load Game"), btn_bg, btn_hov, btn_br, 15, 10.f)) {
        engine_refresh_save_slots(eng, false);
        eng.return_screen = GameScreen::PauseMenu;
        eng.screen = GameScreen::LoadMenu;
    }
    if (ui_button(eng.ui, eng.window, sf::FloatRect({bx, by + step * idx++}, {bw, 40.f}), L(eng, "gallery", "Gallery"), btn_bg, btn_hov, btn_br, 15, 10.f, UiIcon::Image)) {
        engine_refresh_profile(eng);
        eng.gallery_lightbox_idx = -1;
        eng.return_screen = GameScreen::PauseMenu;
        eng.screen = GameScreen::GalleryMenu;
    }
    if (ui_button(eng.ui, eng.window, sf::FloatRect({bx, by + step * idx++}, {bw, 40.f}), L(eng, "achievements", "Achievements"), btn_bg, btn_hov, btn_br, 15, 10.f, UiIcon::Trophy)) {
        engine_refresh_profile(eng);
        eng.return_screen = GameScreen::PauseMenu;
        eng.screen = GameScreen::AchievementsMenu;
    }
    if (eng.config.mods_enabled) {
        if (ui_button(eng.ui, eng.window, sf::FloatRect({bx, by + step * idx++}, {bw, 40.f}), L(eng, "mods", "Mods"), btn_bg, btn_hov, btn_br, 15, 10.f, UiIcon::Layers)) {
            mod_menu_reload(eng.mod_menu, eng.base_dir);
            eng.return_screen = GameScreen::PauseMenu;
            eng.screen = GameScreen::ModsMenu;
        }
    }
    if (ui_button(eng.ui, eng.window, sf::FloatRect({bx, by + step * idx++}, {bw, 40.f}), L(eng, "settings", "Settings"), btn_bg, btn_hov, btn_br, 15, 10.f, UiIcon::Sliders)) {
        eng.return_screen = GameScreen::PauseMenu;
        eng.screen = GameScreen::SettingsMenu;
    }
    if (ui_button(eng.ui, eng.window, sf::FloatRect({bx, by + step * idx++}, {bw, 40.f}), L(eng, "main_menu", "Main Menu"), btn_bg, btn_hov, btn_br, 15, 10.f)) {
        eng.screen = GameScreen::MainMenu;
        if (eng.config.menu_music[0] != '\0') audio_play_music(eng.audio, eng.config.menu_music, true);
    }
    if (ui_button(eng.ui, eng.window, sf::FloatRect({bx, by + step * idx++}, {bw, 40.f}), L(eng, "quit", "Exit"), sf::Color(63, 20, 26, 220), sf::Color(153, 27, 27, 240), sf::Color(248, 113, 113, 85), 15, 10.f)) {
        eng.window.close();
    }
}

static void engine_render_saveload_menu(EngineState& eng, bool is_save) {
    static int save_page = 0;
    sf::RectangleShape dim({LOGICAL_W, LOGICAL_H});
    dim.setFillColor(sf::Color(5, 6, 10, 188));
    eng.window.draw(dim);

    float pw = 880.f;
    float ph = 590.f;
    float px = (LOGICAL_W - pw) * 0.5f;
    float py = (LOGICAL_H - ph) * 0.5f;

    ui_draw_glass_panel(eng.window, sf::FloatRect({px, py}, {pw, ph}), 18.f, to_sf_color(eng.config.theme_panel_color), to_sf_color(eng.config.theme_panel_border_color), 120);
    ui_draw_label(eng.ui, eng.window, is_save ? L(eng, "save_game", "Save Game") : L(eng, "load_game", "Load Game"), {px + 30.f, py + 24.f}, 22, sf::Color(250, 250, 250));

    if (ui_button(eng.ui, eng.window, sf::FloatRect({px + pw - 64.f, py + 20.f}, {36.f, 36.f}), "", sf::Color(255, 255, 255, 10), sf::Color(255, 255, 255, 28), sf::Color(255, 255, 255, 25), 14, 10.f, UiIcon::Close)) {
        eng.screen = eng.return_screen;
        return;
    }

    sf::Color acc = to_sf_color(eng.config.theme_accent_color);
    sf::Color btn_bg = to_sf_color(eng.config.theme_button_color);
    sf::Color btn_hov = to_sf_color(eng.config.theme_button_hover_color);
    sf::Color btn_br = to_sf_color(eng.config.theme_panel_border_color);

    const size_t per_page = 6;
    size_t total_pages = std::max<size_t>(1, (eng.slot_metas.size() + per_page - 1) / per_page);
    save_page = std::clamp(save_page, 0, static_cast<int>(total_pages - 1));

    size_t start_idx = static_cast<size_t>(save_page) * per_page;
    for (size_t r = 0; r < per_page; ++r) {
        size_t i = start_idx + r;
        if (i >= eng.slot_metas.size()) break;

        int col = static_cast<int>(r % 2);
        int row = static_cast<int>(r / 2);
        float cx = px + 30.f + static_cast<float>(col) * 416.f;
        float cy = py + 78.f + static_cast<float>(row) * 122.f;
        sf::FloatRect card({cx, cy}, {404.f, 106.f});

        const auto& meta = eng.slot_metas[i];
        bool hovered = card.contains(eng.ui.mouse_pos);
        float anim = ui_anim_hover(eng.ui, 80000ULL + i, hovered);

        sf::Color bg = anim > 0.2f ? to_sf_color(eng.config.theme_button_hover_color) : to_sf_color(eng.config.theme_button_color);
        sf::Color br = anim > 0.2f ? sf::Color(acc.r, acc.g, acc.b, 180) : to_sf_color(eng.config.theme_panel_border_color);

        ui_draw_glass_panel(eng.window, card, 12.f, bg, br, 50);

        sf::FloatRect thumb_box({cx + 10.f, cy + 12.f}, {146.f, 82.f});
        ui_draw_rounded_panel(eng.window, thumb_box, 2.f, sf::Color(9, 9, 11), sf::Color(40, 40, 46), 1.f, 0);
        if (i < eng.slot_thumbs.size() && eng.slot_thumbs[i].has_value()) {
            sf::Sprite tspr(*eng.slot_thumbs[i]);
            sf::Vector2u tsz = eng.slot_thumbs[i]->getSize();
            if (tsz.x > 0 && tsz.y > 0) {
                tspr.setScale({thumb_box.size.x / static_cast<float>(tsz.x), thumb_box.size.y / static_cast<float>(tsz.y)});
            }
            tspr.setPosition(thumb_box.position);
            eng.window.draw(tspr);
        } else {
            ui_draw_icon(eng.window, UiIcon::Image, {thumb_box.position.x + thumb_box.size.x * 0.5f, thumb_box.position.y + thumb_box.size.y * 0.5f}, 20.f, sf::Color(58, 58, 66));
        }

        float tx = cx + 168.f;
        float tw = 404.f - 180.f;
        bool is_auto = (std::string(meta.slot) == "autosave" || std::string(meta.slot) == "quicksave");
        sf::Color badge_col = is_auto ? sf::Color(245, 158, 11) : acc;
        ui_draw_soft_badge(eng.ui, eng.window, sf::FloatRect({tx, cy + 12.f}, {86.f, 20.f}), meta.slot, badge_col, 11);

        if (meta.exists) {
            ui_draw_label_clamped(eng.ui, eng.window, meta.timestamp, {tx + 94.f, cy + 14.f}, 11, sf::Color(138, 138, 147), tw - 94.f);
            std::string spk = meta.speaker[0] ? std::string(meta.speaker) : "Narrator";
            ui_draw_label_clamped(eng.ui, eng.window, spk, {tx, cy + 40.f}, 12, to_sf_color(eng.config.theme_accent_hover_color), tw);
            ui_draw_label_clamped(eng.ui, eng.window, meta.preview, {tx, cy + 62.f}, 12, sf::Color(228, 228, 231), tw);
        } else {
            ui_draw_label_clamped(eng.ui, eng.window, L(eng, "empty_slot", "Empty Slot"), {tx, cy + 48.f}, 13, sf::Color(113, 113, 122), tw);
        }

        if (hovered && eng.ui.mouse_clicked) {
            ui_consume_click(eng.ui);
            if (is_save) {
                engine_save_slot(eng, meta.slot, "");
                engine_refresh_save_slots(eng, true);
            } else if (meta.exists) {
                engine_load_slot(eng, meta.slot);
                return;
            }
        }
    }

    float pag_y = py + ph - 50.f;
    float pag_x = px + 30.f;
    for (size_t p = 0; p < total_pages; ++p) {
        bool cur = (static_cast<int>(p) == save_page);
        sf::FloatRect pr({pag_x + static_cast<float>(p) * 44.f, pag_y}, {36.f, 32.f});
        if (ui_button(eng.ui, eng.window, pr, std::to_string(p + 1), cur ? acc : btn_bg, cur ? acc : btn_hov, cur ? acc : btn_br, 13, 8.f)) {
            save_page = static_cast<int>(p);
        }
    }
}

static void engine_render_settings_menu(EngineState& eng) {
    sf::RectangleShape dim({LOGICAL_W, LOGICAL_H});
    dim.setFillColor(sf::Color(8, 8, 10, 215));
    eng.window.draw(dim);

    float pw = 720.f;
    float ph = 590.f;
    float px = (LOGICAL_W - pw) * 0.5f;
    float py = (LOGICAL_H - ph) * 0.5f;

    sf::Color acc = to_sf_color(eng.config.theme_accent_color);
    sf::Color panel_bg(12, 12, 14, 250);
    sf::Color panel_br(42, 42, 48);
    sf::Color btn_bg(20, 20, 24);
    sf::Color btn_hov(32, 32, 38);

    ui_draw_rounded_panel(eng.window, sf::FloatRect({px, py}, {pw, ph}), 4.f, panel_bg, panel_br, 1.f, 0);
    ui_draw_label(eng.ui, eng.window, L(eng, "settings", "Settings"), {px + 28.f, py + 20.f}, 20, sf::Color(244, 244, 245));

    if (ui_button(eng.ui, eng.window, sf::FloatRect({px + pw - 58.f, py + 16.f}, {32.f, 32.f}), "", btn_bg, btn_hov, panel_br, 13, 3.f, UiIcon::Close)) {
        eng.screen = eng.return_screen;
        return;
    }
    ui_draw_divider(eng.window, {px, py + 62.f}, pw, sf::Color(32, 32, 38));

    bool changed = false;
    bool apply_win = false;
    bool reload_lang = false;

    auto step_row = [&](float y, const std::string& label, float& val, float step, float min_v, float max_v, const std::string& suffix) {
        ui_draw_label(eng.ui, eng.window, label, {px + 28.f, y + 6.f}, 14, sf::Color(212, 212, 216));
        float rx = px + 240.f;
        if (ui_button(eng.ui, eng.window, sf::FloatRect({rx, y}, {34.f, 28.f}), "-", btn_bg, btn_hov, panel_br, 15, 2.f)) {
            val = std::max(min_v, val - step);
            changed = true;
        }
        std::string vstr = (step < 1.f)
            ? (std::to_string(val).substr(0, 3) + suffix)
            : (std::to_string(static_cast<int>(std::round(val))) + suffix);
        ui_draw_rounded_panel(eng.window, sf::FloatRect({rx + 40.f, y}, {116.f, 28.f}), 2.f, sf::Color(9, 9, 11), sf::Color(34, 34, 40), 1.f, 0);
        ui_draw_label_centered(eng.ui, eng.window, vstr, sf::FloatRect({rx + 40.f, y}, {116.f, 28.f}), 13, sf::Color(244, 244, 245));
        if (ui_button(eng.ui, eng.window, sf::FloatRect({rx + 162.f, y}, {34.f, 28.f}), "+", btn_bg, btn_hov, panel_br, 15, 2.f)) {
            val = std::min(max_v, val + step);
            changed = true;
        }
        float pct = (val - min_v) / std::max(0.001f, max_v - min_v);
        sf::RectangleShape bar_bg({220.f, 4.f});
        bar_bg.setPosition({rx + 216.f, y + 12.f});
        bar_bg.setFillColor(sf::Color(34, 34, 40));
        eng.window.draw(bar_bg);
        sf::RectangleShape bar_fg({std::round(220.f * pct), 4.f});
        bar_fg.setPosition({rx + 216.f, y + 12.f});
        bar_fg.setFillColor(acc);
        eng.window.draw(bar_fg);
    };

    float y = py + 80.f;
    step_row(y, L(eng, "music_volume", "Music Volume"), eng.settings.music_volume, 5.f, 0.f, 100.f, "%");
    y += 38.f;
    step_row(y, L(eng, "sound_volume", "Sound Volume"), eng.settings.sound_volume, 5.f, 0.f, 100.f, "%");
    y += 38.f;
    step_row(y, L(eng, "voice_volume", "Voice Volume"), eng.settings.voice_volume, 5.f, 0.f, 100.f, "%");
    y += 38.f;
    step_row(y, L(eng, "text_speed", "Text Speed"), eng.settings.typewriter_cps, 5.f, 15.f, 120.f, " cps");
    y += 38.f;
    step_row(y, L(eng, "auto_delay", "Auto Delay"), eng.settings.auto_delay, 0.5f, 0.5f, 6.0f, "s");

    y += 48.f;
    ui_draw_label(eng.ui, eng.window, L(eng, "resolution", "Resolution"), {px + 28.f, y + 5.f}, 14, sf::Color(212, 212, 216));
    const uint32_t res_w[4] = {1280, 1600, 1920, 2560};
    const uint32_t res_h[4] = {720, 900, 1080, 1440};
    const char* res_lbl[4] = {"1280x720", "1600x900", "1920x1080", "2560x1440"};
    for (int i = 0; i < 4; ++i) {
        bool active = (eng.settings.res_width == res_w[i]);
        if (ui_button(eng.ui, eng.window, sf::FloatRect({px + 240.f + static_cast<float>(i) * 114.f, y}, {108.f, 30.f}), res_lbl[i], active ? acc : btn_bg, active ? acc : btn_hov, active ? acc : panel_br, 12, 2.f)) {
            eng.settings.res_width = res_w[i];
            eng.settings.res_height = res_h[i];
            changed = true;
            apply_win = true;
        }
    }

    y += 40.f;
    ui_draw_label(eng.ui, eng.window, L(eng, "window_mode", "Display Mode"), {px + 28.f, y + 5.f}, 14, sf::Color(212, 212, 216));
    std::string mode_lbl[3] = {L(eng, "mode_windowed", "Windowed"), L(eng, "mode_borderless", "Borderless"), L(eng, "mode_fullscreen", "Fullscreen")};
    for (int i = 0; i < 3; ++i) {
        bool active = (eng.settings.window_mode == i);
        if (ui_button(eng.ui, eng.window, sf::FloatRect({px + 240.f + static_cast<float>(i) * 152.f, y}, {146.f, 30.f}), mode_lbl[i], active ? acc : btn_bg, active ? acc : btn_hov, active ? acc : panel_br, 12, 2.f)) {
            eng.settings.window_mode = static_cast<uint8_t>(i);
            changed = true;
            apply_win = true;
        }
    }

    y += 40.f;
    ui_draw_label(eng.ui, eng.window, L(eng, "scale_mode", "Aspect Scaling"), {px + 28.f, y + 5.f}, 14, sf::Color(212, 212, 216));
    std::string scale_lbl[2] = {L(eng, "scale_fit", "Fit (Keep Ratio)"), L(eng, "scale_fill", "Fill (Full Window)")};
    for (int i = 0; i < 2; ++i) {
        bool active = (eng.settings.scale_mode == i);
        if (ui_button(eng.ui, eng.window, sf::FloatRect({px + 240.f + static_cast<float>(i) * 228.f, y}, {222.f, 30.f}), scale_lbl[i], active ? acc : btn_bg, active ? acc : btn_hov, active ? acc : panel_br, 12, 2.f)) {
            eng.settings.scale_mode = static_cast<uint8_t>(i);
            changed = true;
            apply_win = true;
        }
    }

    y += 40.f;
    ui_draw_label(eng.ui, eng.window, L(eng, "language", "Language"), {px + 28.f, y + 5.f}, 14, sf::Color(212, 212, 216));
    const char* langs[3] = {"en", "ru", "ja"};
    const char* ja_label = eng.ui.font.hasGlyph(U'日') ? "日本語" : "Japanese (JA)";
    const char* lang_names[3] = {"English", "Русский", ja_label};
    std::string cur_lang = eng.settings.language[0] ? std::string(eng.settings.language) : "en";
    for (int i = 0; i < 3; ++i) {
        bool active = (cur_lang == langs[i]);
        if (ui_button(eng.ui, eng.window, sf::FloatRect({px + 240.f + static_cast<float>(i) * 152.f, y}, {146.f, 30.f}), lang_names[i], active ? acc : btn_bg, active ? acc : btn_hov, active ? acc : panel_br, 12, 2.f)) {
            std::snprintf(eng.settings.language, sizeof(eng.settings.language), "%s", langs[i]);
            changed = true;
            reload_lang = true;
        }
    }

    y += 46.f;
    if (ui_toggle(eng.ui, eng.window, {px + 28.f, y}, L(eng, "typewriter", "Typewriter Effect"), eng.settings.typewriter_enabled != 0, acc)) {
        eng.settings.typewriter_enabled = eng.settings.typewriter_enabled ? 0 : 1;
        changed = true;
    }
    if (ui_toggle(eng.ui, eng.window, {px + 340.f, y}, L(eng, "idle_breathing", "Character Breathing"), eng.settings.idle_breathing != 0, acc)) {
        eng.settings.idle_breathing = eng.settings.idle_breathing ? 0 : 1;
        changed = true;
    }

    if (changed) {
        engine_apply_player_settings(eng);
        std::string save_dir = platform_resolve_save_dir(eng.config.save_dir);
        qvn_settings_save(save_dir.c_str(), &eng.settings);
        if (reload_lang) {
            engine_load_locale(eng);
            std::string lang = eng.settings.language;
            scene_apply_config(eng.scene, eng.config, lang);
            if (lang == "ja" && eng.config.font_ja_path[0]) {
                ui_init(eng.ui, eng.config.font_ja_path);
            } else if (!eng.ui.font.hasGlyph(U'日')) {
                ui_init(eng.ui, "");
            }
        }
        if (apply_win) engine_apply_window_settings(eng);
    }

    if (ui_button(eng.ui, eng.window, sf::FloatRect({px + pw - 142.f, py + ph - 52.f}, {114.f, 34.f}), L(eng, "done", "Done"), acc, acc, acc, 14, 3.f, UiIcon::Check)) {
        eng.screen = eng.return_screen;
    }
}

static void engine_render_backlog(EngineState& eng) {
    sf::RectangleShape dim({LOGICAL_W, LOGICAL_H});
    dim.setFillColor(sf::Color(5, 6, 10, 196));
    eng.window.draw(dim);

    float pw = 920.f;
    float ph = 580.f;
    float px = (LOGICAL_W - pw) * 0.5f;
    float py = (LOGICAL_H - ph) * 0.5f;
    sf::Color acc = to_sf_color(eng.config.theme_accent_color);

    ui_draw_glass_panel(eng.window, sf::FloatRect({px, py}, {pw, ph}), 18.f, to_sf_color(eng.config.theme_panel_color), to_sf_color(eng.config.theme_panel_border_color), 120);
    ui_draw_label(eng.ui, eng.window, L(eng, "history_title", "Dialogue History"), {px + 30.f, py + 24.f}, 22, sf::Color(250, 250, 250));

    if (ui_button(eng.ui, eng.window, sf::FloatRect({px + pw - 64.f, py + 20.f}, {36.f, 36.f}), "", sf::Color(255, 255, 255, 10), sf::Color(255, 255, 255, 28), sf::Color(255, 255, 255, 25), 14, 10.f, UiIcon::Close)) {
        eng.screen = GameScreen::Playing;
        return;
    }

    int max_s = std::max(0, static_cast<int>(eng.backlog.size()) - 8);
    if (eng.ui.wheel_delta != 0.f) {
        eng.backlog_scroll = std::clamp(eng.backlog_scroll - static_cast<int>(eng.ui.wheel_delta), 0, max_s);
    }

    for (int r = 0; r < 8; ++r) {
        size_t idx = static_cast<size_t>(eng.backlog_scroll + r);
        if (idx >= eng.backlog.size()) break;
        float ry = py + 78.f + static_cast<float>(r) * 60.f;
        const auto& entry = eng.backlog[idx];
        ui_draw_rounded_panel(eng.window, sf::FloatRect({px + 28.f, ry}, {pw - 64.f, 52.f}), 10.f, sf::Color(24, 25, 34, 210), sf::Color(255, 255, 255, 16), 1.f, 0);
        if (!entry.speaker.empty()) {
            ui_draw_label(eng.ui, eng.window, entry.speaker, {px + 42.f, ry + 6.f}, 13, to_sf_color(eng.config.theme_accent_hover_color));
        }
        std::string txt = entry.text;
        if (txt.size() > 96) txt = txt.substr(0, 93) + "...";
        ui_draw_label(eng.ui, eng.window, txt, {px + 42.f, ry + 26.f}, 15, sf::Color(228, 228, 231));
    }

    if (max_s > 0) {
        float track_h = ph - 104.f;
        float thumb_h = std::max(36.f, track_h * (8.f / static_cast<float>(eng.backlog.size())));
        float ratio = static_cast<float>(eng.backlog_scroll) / static_cast<float>(max_s);
        float thumb_y = py + 78.f + ratio * (track_h - thumb_h);
        ui_draw_rounded_panel(eng.window, sf::FloatRect({px + pw - 16.f, py + 78.f}, {4.f, track_h}), 2.f, sf::Color(255, 255, 255, 12), sf::Color::Transparent, 0.f, 0);
        ui_draw_rounded_panel(eng.window, sf::FloatRect({px + pw - 16.f, thumb_y}, {4.f, thumb_h}), 2.f, acc, sf::Color::Transparent, 0.f, 0);
    }
}

static void engine_render_gallery_menu(EngineState& eng) {
    static int gallery_page = 0;

    if (eng.gallery_lightbox_idx >= 0 && static_cast<size_t>(eng.gallery_lightbox_idx) < eng.gallery_items.size()) {
        const auto& item = eng.gallery_items[static_cast<size_t>(eng.gallery_lightbox_idx)];
        if (item.tex_loaded) {
            sf::Sprite spr(item.texture);
            sf::Vector2u sz = item.texture.getSize();
            if (sz.x > 0 && sz.y > 0) spr.setScale({LOGICAL_W / static_cast<float>(sz.x), LOGICAL_H / static_cast<float>(sz.y)});
            eng.window.draw(spr);
        }
        if (ui_button(eng.ui, eng.window, sf::FloatRect({LOGICAL_W - 64.f, 20.f}, {40.f, 40.f}), "", sf::Color(15, 16, 22, 200), sf::Color(35, 38, 52, 230), sf::Color(255, 255, 255, 45), 16, 12.f, UiIcon::Close) || eng.ui.mouse_clicked) {
            ui_consume_click(eng.ui);
            eng.gallery_lightbox_idx = -1;
        }
        return;
    }

    sf::RectangleShape dim({LOGICAL_W, LOGICAL_H});
    dim.setFillColor(sf::Color(5, 6, 10, 196));
    eng.window.draw(dim);

    float pw = 960.f;
    float ph = 600.f;
    float px = (LOGICAL_W - pw) * 0.5f;
    float py = (LOGICAL_H - ph) * 0.5f;

    ui_draw_glass_panel(eng.window, sf::FloatRect({px, py}, {pw, ph}), 18.f, to_sf_color(eng.config.theme_panel_color), to_sf_color(eng.config.theme_panel_border_color), 120);
    ui_draw_label(eng.ui, eng.window, L(eng, "gallery", "Gallery"), {px + 30.f, py + 24.f}, 22, sf::Color(250, 250, 250));

    if (ui_button(eng.ui, eng.window, sf::FloatRect({px + pw - 64.f, py + 20.f}, {36.f, 36.f}), "", sf::Color(255, 255, 255, 10), sf::Color(255, 255, 255, 28), sf::Color(255, 255, 255, 25), 14, 10.f, UiIcon::Close)) {
        eng.screen = eng.return_screen;
        return;
    }

    sf::Color acc = to_sf_color(eng.config.theme_accent_color);
    sf::Color btn_bg = to_sf_color(eng.config.theme_button_color);
    sf::Color btn_hov = to_sf_color(eng.config.theme_button_hover_color);
    sf::Color btn_br = to_sf_color(eng.config.theme_panel_border_color);

    const size_t per_page = 6;
    size_t total_pages = std::max<size_t>(1, (eng.gallery_items.size() + per_page - 1) / per_page);
    gallery_page = std::clamp(gallery_page, 0, static_cast<int>(total_pages - 1));

    size_t start_idx = static_cast<size_t>(gallery_page) * per_page;
    for (size_t r = 0; r < per_page; ++r) {
        size_t i = start_idx + r;
        if (i >= eng.gallery_items.size()) break;

        int col = static_cast<int>(r % 3);
        int row = static_cast<int>(r / 3);
        float cx = px + 30.f + static_cast<float>(col) * 308.f;
        float cy = py + 82.f + static_cast<float>(row) * 224.f;
        sf::FloatRect card({cx, cy}, {288.f, 206.f});

        const auto& item = eng.gallery_items[i];
        bool hovered = card.contains(eng.ui.mouse_pos);
        ui_draw_glass_panel(eng.window, card, 14.f, sf::Color(22, 24, 32), hovered && item.def.unlocked ? acc : sf::Color(255, 255, 255, 24), 40);

        if (item.def.unlocked && item.tex_loaded) {
            sf::Sprite spr(item.texture);
            sf::Vector2u sz = item.texture.getSize();
            if (sz.x > 0 && sz.y > 0) spr.setScale({268.f / static_cast<float>(sz.x), 148.f / static_cast<float>(sz.y)});
            spr.setPosition({cx + 10.f, cy + 10.f});
            eng.window.draw(spr);
            ui_draw_label(eng.ui, eng.window, item.def.title, {cx + 14.f, cy + 168.f}, 15, sf::Color(244, 244, 245));
            if (hovered && eng.ui.mouse_clicked) {
                ui_consume_click(eng.ui);
                eng.gallery_lightbox_idx = static_cast<int>(i);
            }
        } else {
            ui_draw_icon(eng.window, UiIcon::Image, {cx + 144.f, cy + 84.f}, 34.f, sf::Color(82, 82, 91));
            ui_draw_label_centered(eng.ui, eng.window, L(eng, "locked", "Locked"), sf::FloatRect({cx, cy + 162.f}, {288.f, 28.f}), 14, sf::Color(113, 113, 122));
        }
    }

    float pag_y = py + ph - 50.f;
    float pag_x = px + 30.f;
    for (size_t p = 0; p < total_pages; ++p) {
        bool cur = (static_cast<int>(p) == gallery_page);
        sf::FloatRect pr({pag_x + static_cast<float>(p) * 44.f, pag_y}, {36.f, 32.f});
        if (ui_button(eng.ui, eng.window, pr, std::to_string(p + 1), cur ? acc : btn_bg, cur ? acc : btn_hov, cur ? acc : btn_br, 13, 8.f)) {
            gallery_page = static_cast<int>(p);
        }
    }
}

static void engine_render_achievements_menu(EngineState& eng) {
    static int ach_scroll = 0;
    sf::RectangleShape dim({LOGICAL_W, LOGICAL_H});
    dim.setFillColor(sf::Color(8, 8, 10, 210));
    eng.window.draw(dim);

    float pw = 760.f;
    float ph = 560.f;
    float px = (LOGICAL_W - pw) * 0.5f;
    float py = (LOGICAL_H - ph) * 0.5f;
    sf::Color acc = to_sf_color(eng.config.theme_accent_color);

    size_t unlocked_cnt = 0;
    for (const auto& a : eng.achievement_items) if (a.unlocked) unlocked_cnt++;

    ui_draw_rounded_panel(eng.window, sf::FloatRect({px, py}, {pw, ph}), 4.f, sf::Color(12, 12, 14, 248), sf::Color(42, 42, 48), 1.f, 0);
    std::string header = L(eng, "achievements", "Achievements");
    ui_draw_label(eng.ui, eng.window, header, {px + 28.f, py + 22.f}, 20, sf::Color(244, 244, 245));
    float title_w = ui_measure_text_width(eng.ui.font, header, 20);
    std::string cnt_str = std::to_string(unlocked_cnt) + " / " + std::to_string(eng.achievement_items.size());
    float badge_w = std::max(54.f, ui_measure_text_width(eng.ui.font, cnt_str, 12) + 20.f);
    ui_draw_soft_badge(eng.ui, eng.window, sf::FloatRect({px + 28.f + title_w + 14.f, py + 23.f}, {badge_w, 22.f}), cnt_str, sf::Color(250, 204, 21), 12);

    if (ui_button(eng.ui, eng.window, sf::FloatRect({px + pw - 58.f, py + 18.f}, {32.f, 32.f}), "", sf::Color(20, 20, 24), sf::Color(34, 34, 40), sf::Color(42, 42, 48), 13, 3.f, UiIcon::Close)) {
        eng.screen = eng.return_screen;
        return;
    }

    ui_draw_divider(eng.window, {px, py + 66.f}, pw, sf::Color(32, 32, 38));

    const int visible = 5;
    int max_s = std::max(0, static_cast<int>(eng.achievement_items.size()) - visible);
    if (eng.ui.wheel_delta != 0.f) {
        ach_scroll = std::clamp(ach_scroll - static_cast<int>(eng.ui.wheel_delta), 0, max_s);
    }

    for (int r = 0; r < visible; ++r) {
        size_t i = static_cast<size_t>(ach_scroll + r);
        if (i >= eng.achievement_items.size()) break;
        const auto& a = eng.achievement_items[i];
        float ry = py + 80.f + static_cast<float>(r) * 86.f;

        std::string tkey = std::string("ach_") + a.id + "_title";
        std::string dkey = std::string("ach_") + a.id + "_desc";
        std::string title = L(eng, tkey.c_str(), a.title);
        std::string desc = L(eng, dkey.c_str(), a.description);

        sf::Color card_br = a.unlocked ? sf::Color(250, 204, 21, 120) : sf::Color(34, 34, 40);
        ui_draw_rounded_panel(eng.window, sf::FloatRect({px + 24.f, ry}, {pw - 60.f, 72.f}), 3.f, sf::Color(15, 15, 18), card_br, 1.f, 0);
        ui_draw_icon(eng.window, UiIcon::Trophy, {px + 54.f, ry + 36.f}, 20.f, a.unlocked ? sf::Color(250, 204, 21) : sf::Color(82, 82, 91));
        ui_draw_label_clamped(eng.ui, eng.window, title, {px + 84.f, ry + 14.f}, 15, a.unlocked ? sf::Color(244, 244, 245) : sf::Color(161, 161, 170), pw - 250.f);
        ui_draw_label_clamped(eng.ui, eng.window, desc, {px + 84.f, ry + 40.f}, 13, sf::Color(113, 113, 122), pw - 250.f);
        ui_draw_soft_badge(eng.ui, eng.window, sf::FloatRect({px + pw - 142.f, ry + 24.f}, {92.f, 24.f}), a.unlocked ? L(eng, "unlocked", "Unlocked") : L(eng, "locked", "Locked"), a.unlocked ? sf::Color(16, 185, 129) : sf::Color(113, 113, 122), 11);
    }

    if (max_s > 0) {
        ui_draw_scrollbar(eng.window, sf::FloatRect({px + pw - 16.f, py + 80.f}, {4.f, ph - 104.f}), static_cast<int>(eng.achievement_items.size()), visible, ach_scroll, acc);
    }
}

static void engine_render_prompt_modal(EngineState& eng) {
    sf::RectangleShape dim({LOGICAL_W, LOGICAL_H});
    dim.setFillColor(sf::Color(5, 6, 10, 185));
    eng.window.draw(dim);

    float pw = 480.f;
    float ph = 220.f;
    float px = (LOGICAL_W - pw) * 0.5f;
    float py = (LOGICAL_H - ph) * 0.5f;
    sf::Color acc = to_sf_color(eng.config.theme_accent_color);

    std::string label = eng.prompt_text.empty() ? L(eng, "enter_name_title", "What is your name?") : eng.prompt_text;
    std::string placeholder = L(eng, "enter_name_placeholder", "Type your name here...");

    ui_draw_glass_panel(eng.window, sf::FloatRect({px, py}, {pw, ph}), 18.f, to_sf_color(eng.config.theme_panel_color), to_sf_color(eng.config.theme_panel_border_color), 120);
    ui_draw_label(eng.ui, eng.window, label, {px + 28.f, py + 26.f}, 18, sf::Color(250, 250, 250));
    ui_text_input(eng.ui, eng.window, 555, sf::FloatRect({px + 28.f, py + 76.f}, {pw - 56.f, 44.f}), eng.prompt_buffer, placeholder, acc);

    if (ui_button(eng.ui, eng.window, sf::FloatRect({px + pw - 156.f, py + 150.f}, {128.f, 40.f}), L(eng, "done", "Done"), sf::Color(79, 70, 229), acc, acc, 15, 10.f)) {
        qjs_set_state_var(eng.script, eng.prompt_state_key.c_str(), eng.prompt_buffer.c_str());
        eng.screen = GameScreen::Playing;
        engine_execute_next_command(eng);
    }
}

static void engine_render_end_screen(EngineState& eng) {
    ui_draw_gradient_rect(eng.window, sf::FloatRect({0.f, 0.f}, {LOGICAL_W, LOGICAL_H}), sf::Color(9, 10, 15, 215), sf::Color(18, 20, 32, 235));

    float pw = 460.f;
    float ph = 320.f;
    float px = (LOGICAL_W - pw) * 0.5f;
    float py = (LOGICAL_H - ph) * 0.5f;

    sf::Color acc = to_sf_color(eng.config.theme_accent_color);
    sf::Color btn_bg = to_sf_color(eng.config.theme_button_color);
    sf::Color btn_hov = to_sf_color(eng.config.theme_button_hover_color);
    sf::Color btn_br = to_sf_color(eng.config.theme_panel_border_color);

    ui_draw_glass_panel(eng.window, sf::FloatRect({px, py}, {pw, ph}), 18.f, to_sf_color(eng.config.theme_panel_color), to_sf_color(eng.config.theme_panel_border_color), 120);
    ui_draw_label_centered(eng.ui, eng.window, eng.ending_title, sf::FloatRect({px, py + 34.f}, {pw, 46.f}), 36, sf::Color(250, 250, 250));
    ui_draw_label_centered(eng.ui, eng.window, eng.ending_subtitle, sf::FloatRect({px, py + 86.f}, {pw, 28.f}), 16, sf::Color(161, 161, 170));

    if (ui_button(eng.ui, eng.window, sf::FloatRect({px + 40.f, py + 148.f}, {pw - 80.f, 44.f}), L(eng, "main_menu", "Main Menu"), sf::Color(79, 70, 229), acc, acc, 16, 10.f)) {
        eng.screen = GameScreen::MainMenu;
        if (eng.config.menu_music[0] != '\0') audio_play_music(eng.audio, eng.config.menu_music, true);
    }
    if (ui_button(eng.ui, eng.window, sf::FloatRect({px + 40.f, py + 204.f}, {184.f, 42.f}), L(eng, "load_game", "Load Game"), btn_bg, btn_hov, btn_br, 15, 10.f)) {
        engine_refresh_save_slots(eng, false);
        eng.return_screen = GameScreen::EndScreen;
        eng.screen = GameScreen::LoadMenu;
    }
    if (ui_button(eng.ui, eng.window, sf::FloatRect({px + pw - 224.f, py + 204.f}, {184.f, 42.f}), L(eng, "gallery", "Gallery"), btn_bg, btn_hov, btn_br, 15, 10.f, UiIcon::Image)) {
        engine_refresh_profile(eng);
        eng.gallery_lightbox_idx = -1;
        eng.return_screen = GameScreen::EndScreen;
        eng.screen = GameScreen::GalleryMenu;
    }
}

static void engine_render_dev_inspector(EngineState& eng) {
    if (!eng.dev_inspector) return;
    ui_draw_glass_panel(eng.window, sf::FloatRect({18.f, 18.f}, {360.f, 178.f}), 12.f, sf::Color(9, 10, 15, 225), sf::Color(99, 102, 241, 140), 50);
    ui_draw_label(eng.ui, eng.window, "Dev Inspector (F10)", {32.f, 28.f}, 14, sf::Color(129, 140, 248));
    ui_draw_label(eng.ui, eng.window, "FPS: " + std::to_string(static_cast<int>(eng.dev_fps)) + "  |  Cmd: " + std::to_string(eng.current_command_idx) + "/" + std::to_string(eng.commands.size()), {32.f, 54.f}, 12, sf::Color(228, 228, 231));
    ui_draw_label(eng.ui, eng.window, "Mods active: " + std::to_string(eng.active_mods.size()) + "  |  Script: " + std::string(eng.config.entry_script), {32.f, 76.f}, 12, sf::Color(161, 161, 170));

    char* js_state = qjs_serialize_state(eng.script);
    std::string st_str = js_state ? js_state : "{}";
    if (js_state) qjs_free_cstr(js_state);
    if (st_str.size() > 46) st_str = st_str.substr(0, 43) + "...";
    ui_draw_label(eng.ui, eng.window, "State: " + st_str, {32.f, 98.f}, 12, sf::Color(52, 211, 153));
    ui_draw_label(eng.ui, eng.window, "Chars: " + std::to_string(eng.scene.characters.size()) + "  |  Zoom: " + std::to_string(eng.scene.cam_zoom).substr(0, 4), {32.f, 120.f}, 12, sf::Color(161, 161, 170));
    ui_draw_label(eng.ui, eng.window, "Res: " + std::to_string(eng.window.getSize().x) + "x" + std::to_string(eng.window.getSize().y), {32.f, 142.f}, 12, sf::Color(161, 161, 170));
}

static void engine_process_events(EngineState& eng) {
    while (const std::optional event = eng.window.pollEvent()) {
        if (event->is<sf::Event::Closed>()) {
            eng.window.close();
            continue;
        }
        if (const auto* resized = event->getIf<sf::Event::Resized>()) {
            eng.window.setView(platform_compute_view(resized->size, LOGICAL_W, LOGICAL_H, eng.settings.scale_mode));
            continue;
        }

        ui_handle_event(eng.ui, *event, eng.window);

        if (const auto* key_any = event->getIf<sf::Event::KeyPressed>()) {
            if (key_any->code == sf::Keyboard::Key::F10 && eng.config.dev_inspector_enabled) {
                eng.dev_inspector = !eng.dev_inspector;
                continue;
            }
        }

        if (eng.screen == GameScreen::PromptModal) {
            if (const auto* key = event->getIf<sf::Event::KeyPressed>()) {
                if (key->code == sf::Keyboard::Key::Enter) {
                    qjs_set_state_var(eng.script, eng.prompt_state_key.c_str(), eng.prompt_buffer.c_str());
                    eng.screen = GameScreen::Playing;
                    engine_execute_next_command(eng);
                }
            }
            continue;
        }

        if (eng.screen != GameScreen::Playing) {
            if (const auto* key = event->getIf<sf::Event::KeyPressed>()) {
                if (key->code == eng.key_exit && eng.key_exit != sf::Keyboard::Key::Unknown) {
                    if (eng.screen == GameScreen::GalleryMenu && eng.gallery_lightbox_idx >= 0) {
                        eng.gallery_lightbox_idx = -1;
                    } else if (eng.screen == GameScreen::PauseMenu || eng.screen == GameScreen::BacklogMenu) {
                        eng.screen = GameScreen::Playing;
                    } else if (eng.screen != GameScreen::MainMenu && eng.screen != GameScreen::EndScreen) {
                        eng.screen = eng.return_screen;
                    }
                }
            }
            continue;
        }

        if (const auto* mouse_moved = event->getIf<sf::Event::MouseMoved>()) {
            if (eng.waiting_for_choice) scene_update_choice_hover(eng.scene, platform_map_pixel(eng.window, mouse_moved->position));
        } else if (const auto* mouse_btn = event->getIf<sf::Event::MouseButtonPressed>()) {
            if (mouse_btn->button == sf::Mouse::Button::Right) {
                eng.ui_hidden = !eng.ui_hidden;
            } else if (mouse_btn->button == sf::Mouse::Button::Left) {
                sf::Vector2f pos = platform_map_pixel(eng.window, mouse_btn->position);
                if (eng.config.hud_enabled && !eng.ui_hidden && engine_hud_bar_rect(eng).contains(pos)) {
                    continue;
                }
                ui_consume_click(eng.ui);
                if (eng.waiting_for_choice) {
                    int selected = scene_get_choice_at(eng.scene, pos);
                    if (selected >= 0) engine_pick_choice(eng, selected);
                } else {
                    engine_handle_advance(eng);
                }
            }
        } else if (const auto* touch_began = event->getIf<sf::Event::TouchBegan>()) {
            sf::Vector2f pos = platform_map_pixel(eng.window, touch_began->position);
            if (eng.config.hud_enabled && !eng.ui_hidden && engine_hud_bar_rect(eng).contains(pos)) {
                continue;
            }
            ui_consume_click(eng.ui);
            if (eng.waiting_for_choice) {
                scene_update_choice_hover(eng.scene, pos);
                int selected = scene_get_choice_at(eng.scene, pos);
                if (selected >= 0) engine_pick_choice(eng, selected);
            } else {
                engine_handle_advance(eng);
            }
        } else if (const auto* key = event->getIf<sf::Event::KeyPressed>()) {
            if (eng.waiting_for_choice && key->code >= sf::Keyboard::Key::Num1 && key->code <= sf::Keyboard::Key::Num9) {
                int idx = static_cast<int>(key->code) - static_cast<int>(sf::Keyboard::Key::Num1);
                if (idx >= 0 && static_cast<size_t>(idx) < eng.scene.choice_buttons.size()) {
                    engine_pick_choice(eng, idx);
                    continue;
                }
            }
            if (key->code == eng.key_exit && eng.key_exit != sf::Keyboard::Key::Unknown) {
                engine_capture_scene_thumb(eng);
                eng.screen = GameScreen::PauseMenu;
            } else if (key->code == eng.key_quicksave && eng.key_quicksave != sf::Keyboard::Key::Unknown) {
                engine_save_slot(eng, "quicksave", L(eng, "quicksaved", "Quicksaved"));
            } else if (key->code == eng.key_quickload && eng.key_quickload != sf::Keyboard::Key::Unknown) {
                engine_load_latest(eng);
            } else if (key->code == eng.key_reload && eng.key_reload != sf::Keyboard::Key::Unknown) {
                engine_hot_reload(eng, true);
            } else if (key->code == eng.key_backlog && eng.key_backlog != sf::Keyboard::Key::Unknown) {
                eng.backlog_scroll = std::max(0, static_cast<int>(eng.backlog.size()) - 8);
                eng.return_screen = GameScreen::Playing;
                eng.screen = GameScreen::BacklogMenu;
            } else if (key->code == eng.key_auto_mode && eng.key_auto_mode != sf::Keyboard::Key::Unknown) {
                eng.auto_mode = !eng.auto_mode;
                if (eng.auto_mode) eng.skip_mode = false;
                scene_show_toast(eng.scene, eng.auto_mode ? "Auto-Play ON" : "Auto-Play OFF");
            } else if (key->code == eng.key_skip_mode && eng.key_skip_mode != sf::Keyboard::Key::Unknown) {
                eng.skip_mode = !eng.skip_mode;
                if (eng.skip_mode) eng.auto_mode = false;
                scene_show_toast(eng.scene, eng.skip_mode ? "Skip ON" : "Skip OFF");
            } else if (key->code == eng.key_hide_ui && eng.key_hide_ui != sf::Keyboard::Key::Unknown) {
                eng.ui_hidden = !eng.ui_hidden;
            } else if (
                (key->code == eng.key_advance && eng.key_advance != sf::Keyboard::Key::Unknown) ||
                (key->code == eng.key_advance_alt && eng.key_advance_alt != sf::Keyboard::Key::Unknown)
            ) {
                engine_handle_advance(eng);
            }
        }
    }
}

static void engine_poll_hot_reload(EngineState& eng, float dt) {
    if (!eng.config.hot_reload_enabled) return;
    eng.hot_reload_timer += dt;
    float interval_sec = static_cast<float>(eng.config.hot_reload_interval_ms) * 0.001f;
    if (eng.hot_reload_timer < interval_sec) return;
    eng.hot_reload_timer = 0.f;

    std::string cfg_resolved = platform_resolve_asset_path(eng.config_path);
    std::string script_resolved = platform_resolve_asset_path(eng.config.entry_script);
    std::string mods_resolved = (std::filesystem::path(eng.base_dir) / "mods.json").string();

    uint64_t new_cfg_fp = qvn_file_fingerprint(cfg_resolved.c_str());
    uint64_t new_script_fp = qvn_file_fingerprint(script_resolved.c_str());
    uint64_t new_mods_fp = qvn_file_fingerprint(mods_resolved.c_str());

    bool cfg_changed = (new_cfg_fp != 0 && new_cfg_fp != eng.config_fingerprint);
    bool script_changed = (new_script_fp != 0 && new_script_fp != eng.script_fingerprint);
    bool mods_changed = (new_mods_fp != 0 && new_mods_fp != eng.mods_fingerprint);

    if (cfg_changed || script_changed || mods_changed) {
        engine_hot_reload(eng, cfg_changed || mods_changed);
    }
}

static void engine_update(EngineState& eng, float dt) {
    if (dt > 0.0001f) eng.dev_fps = qvn_lerp(eng.dev_fps, 1.f / dt, 0.1f);
    audio_update(eng.audio, dt);
    engine_poll_hot_reload(eng, dt);

    if (eng.scene.toast_timer > 0.f) eng.scene.toast_timer = std::max(0.f, eng.scene.toast_timer - dt);
    if (eng.scene.ach_toast_timer > 0.f) eng.scene.ach_toast_timer = std::max(0.f, eng.scene.ach_toast_timer - dt);

    if (eng.screen != GameScreen::Playing) return;

    scene_update(eng.scene, eng.config, eng.audio, dt);

    if (eng.waiting_for_choice && eng.scene.choice_timer_total > 0.f) {
        eng.scene.choice_timer_remaining = std::max(0.f, eng.scene.choice_timer_remaining - dt);
        if (eng.scene.choice_timer_remaining <= 0.f) {
            int def_idx = std::clamp(eng.timed_choice_default_idx, 0, std::max(0, static_cast<int>(eng.scene.choice_buttons.size()) - 1));
            engine_pick_choice(eng, def_idx);
            return;
        }
    }

    if (eng.wait_timer > 0.f) {
        eng.wait_timer -= dt;
        if (eng.wait_timer <= 0.f) {
            eng.wait_timer = 0.f;
            engine_execute_next_command(eng);
        }
        return;
    }

    if (eng.skip_mode && !eng.waiting_for_choice) {
        eng.skip_timer += dt;
        float skip_sec = std::max(0.02f, static_cast<float>(eng.config.skip_interval_ms) * 0.001f);
        if (eng.skip_timer >= skip_sec) {
            eng.skip_timer = 0.f;
            if (scene_is_typing(eng.scene)) scene_skip_typewriter(eng.scene);
            else if (eng.waiting_for_click) engine_execute_next_command(eng);
        }
    } else if (eng.auto_mode && !eng.waiting_for_choice && eng.waiting_for_click && !scene_is_typing(eng.scene)) {
        eng.auto_timer += dt;
        if (eng.auto_timer >= eng.config.auto_delay) {
            eng.auto_timer = 0.f;
            engine_execute_next_command(eng);
        }
    }
}

static void engine_render_overlay_toast(EngineState& eng) {
    if (eng.screen == GameScreen::Playing) return;
    if (eng.scene.toast_timer <= 0.f || eng.scene.toast_text.empty()) return;
    float alpha_f = std::min(1.f, eng.scene.toast_timer / 0.35f);
    uint8_t a = static_cast<uint8_t>(std::clamp(alpha_f * 235.f, 0.f, 255.f));
    sf::FloatRect toast_rect({(LOGICAL_W * 0.5f) - 150.f, 24.f}, {300.f, 36.f});
    ui_draw_glass_panel(
        eng.window,
        toast_rect,
        18.f,
        sf::Color(15, 16, 22, a),
        sf::Color(255, 255, 255, static_cast<uint8_t>(a / 6)),
        static_cast<uint8_t>(alpha_f * 70.f)
    );
    ui_draw_label_centered(eng.ui, eng.window, eng.scene.toast_text, toast_rect, 14, sf::Color(244, 244, 245, a));
}

static void engine_render(EngineState& eng) {
    eng.window.clear();
    if (eng.screen == GameScreen::MainMenu) {
        engine_render_main_menu(eng, true);
    } else if (eng.screen == GameScreen::EndScreen) {
        scene_render(eng.scene, eng.config, eng.ui, eng.window, false);
        engine_render_end_screen(eng);
    } else {
        if (eng.return_screen == GameScreen::MainMenu && eng.screen != GameScreen::Playing && eng.screen != GameScreen::PauseMenu) {
            engine_render_main_menu(eng, false);
        } else {
            bool show_dlg = !eng.ui_hidden && (eng.waiting_for_click || eng.waiting_for_choice);
            scene_render(eng.scene, eng.config, eng.ui, eng.window, show_dlg);
            engine_render_hud(eng);
        }

        if (eng.screen == GameScreen::PauseMenu) {
            engine_render_pause_menu(eng);
        } else if (eng.screen == GameScreen::SaveMenu) {
            engine_render_saveload_menu(eng, true);
        } else if (eng.screen == GameScreen::LoadMenu) {
            engine_render_saveload_menu(eng, false);
        } else if (eng.screen == GameScreen::SettingsMenu) {
            engine_render_settings_menu(eng);
        } else if (eng.screen == GameScreen::BacklogMenu) {
            engine_render_backlog(eng);
        } else if (eng.screen == GameScreen::GalleryMenu) {
            engine_render_gallery_menu(eng);
        } else if (eng.screen == GameScreen::AchievementsMenu) {
            engine_render_achievements_menu(eng);
        } else if (eng.screen == GameScreen::ModsMenu) {
            ModMenuAction act = mod_menu_render(eng.mod_menu, eng.ui, eng.window, eng.config, eng.active_locale_json, eng.base_dir);
            if (act == ModMenuAction::Close) {
                eng.screen = eng.return_screen;
            } else if (act == ModMenuAction::ApplyAndReload) {
                mod_menu_save(eng.mod_menu, eng.base_dir);
                engine_hot_reload(eng, true);
                mod_menu_reload(eng.mod_menu, eng.base_dir);
                scene_show_toast(eng.scene, L(eng, "mods_applied", "Mods applied"));
            } else if (act == ModMenuAction::TestModCreated) {
                scene_show_toast(eng.scene, L(eng, "mods_demo_created", "Test mod created"));
            }
        } else if (eng.screen == GameScreen::PromptModal) {
            engine_render_prompt_modal(eng);
        }
    }
    engine_render_overlay_toast(eng);
    engine_render_dev_inspector(eng);
    eng.window.display();
}

bool engine_init(EngineState& eng, const std::string& config_path) {
    eng.config_path = config_path;
    std::filesystem::path cfg_p(config_path);
    std::filesystem::path parent = cfg_p.parent_path();
    eng.base_dir = parent.empty() ? "." : parent.string();

    engine_mount_vfs_and_mods(eng);
    engine_load_config(eng);
    engine_apply_window_settings(eng);

    std::string lang = eng.settings.language[0] ? std::string(eng.settings.language) : "en";
    std::string ui_font_target = (lang == "ja" && eng.config.font_ja_path[0]) ? eng.config.font_ja_path : eng.config.font_path;
    if (!ui_init(eng.ui, ui_font_target)) {
        if (!ui_init(eng.ui, "")) return false;
    }
    if (!scene_init(eng.scene, eng.config, lang)) return false;

    audio_init(eng.audio, eng.config);

    eng.script = qjs_create(engine_on_js_command, &eng);
    if (!eng.script) {
        std::cerr << "Engine Error: Failed to create QuickJS bridge." << std::endl;
        return false;
    }
    qjs_set_language(eng.script, lang.c_str());

    if (eng.config.menu_enabled) {
        eng.screen = GameScreen::MainMenu;
        if (eng.config.menu_music[0] != '\0') audio_play_music(eng.audio, eng.config.menu_music, true, 0.4f);
    } else {
        if (!engine_start_new_game(eng)) return false;
    }

    eng.frame_clock.restart();
    return true;
}

void engine_run(EngineState& eng) {
    while (eng.window.isOpen()) {
        float dt = eng.frame_clock.restart().asSeconds();
        if (dt > 0.1f) dt = 0.1f;
        ui_reset_frame(eng.ui, dt);
        engine_process_events(eng);
        engine_update(eng, dt);
        engine_render(eng);
    }
}

void engine_shutdown(EngineState& eng) {
    audio_shutdown(eng.audio);
    if (eng.script) {
        qjs_destroy(eng.script);
        eng.script = nullptr;
    }
    eng.commands.clear();
    eng.choice_history.clear();
    eng.active_mods.clear();
    eng.mod_menu.items.clear();
    eng.gallery_items.clear();
    eng.achievement_items.clear();
    qvn_vfs_shutdown();
}