#include "tools/studio_state.hpp"
#include "platform/platform.hpp"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>

const char* studio_tr(int lang, const char* en, const char* ru, const char* ja) {
    if (lang == 1) return ru;
    if (lang == 2) return ja;
    return en;
}

void studio_log(LauncherState& st, const std::string& msg) {
    std::lock_guard<std::mutex> lock(st.log_mutex);
    st.logs.push_back(msg);
    if (st.logs.size() > 300) st.logs.erase(st.logs.begin());
}

void studio_set_status(LauncherState& st, const std::string& status) {
    std::lock_guard<std::mutex> lock(st.log_mutex);
    st.status = status;
}

std::string studio_host_platform_id() {
#if defined(_WIN32)
    return "windows";
#elif defined(__APPLE__)
    return "macos";
#else
    return "linux";
#endif
}

std::string studio_find_source_root(const std::string& self_dir) {
    std::error_code ec;
    std::filesystem::path p(self_dir);
    for (int i = 0; i < 4; ++i) {
        if (std::filesystem::is_regular_file(p / "CMakeLists.txt", ec)) {
            return p.string();
        }
        if (!p.has_parent_path() || p.parent_path() == p) break;
        p = p.parent_path();
    }
    return "";
}

std::string studio_resolve_runtime_bin(const LauncherState& st, const std::string& platform_id, const std::string& configured_bin) {
    std::error_code ec;
    std::filesystem::path sd(st.self_dir);
    std::filesystem::path rt(st.runtimes_dir);

    std::string exe_name = "quickjs_vn";
    if (platform_id == "windows") exe_name = "quickjs_vn.exe";
    else if (platform_id == "android") exe_name = "libquickjs_vn.so";

    std::vector<std::filesystem::path> candidates;
    if (!configured_bin.empty()) {
        std::filesystem::path cfg_p(configured_bin);
        candidates.push_back(cfg_p);
        if (cfg_p.is_relative()) candidates.push_back(sd / cfg_p);
    }
    candidates.push_back(rt / platform_id / exe_name);
    candidates.push_back(sd / "runtimes" / platform_id / exe_name);
    candidates.push_back(sd / exe_name);

    for (const auto& c : candidates) {
        if (std::filesystem::is_regular_file(c, ec)) {
            std::filesystem::path canon = std::filesystem::weakly_canonical(c, ec);
            return canon.empty() ? c.string() : canon.string();
        }
    }
    return "";
}

std::string studio_asset_category_root(int filter_idx) {
    static const char* roots[7] = {
        "assets/bg",
        "assets/chars",
        "assets/ui",
        "assets/audio/music",
        "assets/audio/sfx",
        "assets/audio/voice",
        "assets/fonts"
    };
    return roots[std::clamp(filter_idx, 0, 6)];
}

static size_t count_files_recursive(const std::filesystem::path& dir) {
    std::error_code ec;
    if (!std::filesystem::is_directory(dir, ec)) return 0;
    size_t n = 0;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(dir, ec)) {
        if (entry.is_regular_file(ec)) n++;
    }
    return n;
}

void studio_scan_assets(LauncherState& st) {
    st.assets.clear();
    st.asset_scroll = 0;
    st.total_asset_files = 0;
    if (st.active_project_dir.empty()) return;

    std::error_code ec;
    std::filesystem::path proj(st.active_project_dir);
    st.total_asset_files = count_files_recursive(proj / "assets");

    std::filesystem::path base = proj / studio_asset_category_root(st.asset_filter_idx);
    if (!st.asset_subpath.empty()) base /= st.asset_subpath;
    if (!std::filesystem::is_directory(base, ec)) {
        st.asset_subpath.clear();
        base = proj / studio_asset_category_root(st.asset_filter_idx);
    }
    if (!std::filesystem::is_directory(base, ec)) return;

    for (const auto& entry : std::filesystem::directory_iterator(base, ec)) {
        StudioAssetItem item;
        item.is_dir = entry.is_directory(ec);
        if (!item.is_dir && !entry.is_regular_file(ec)) continue;
        item.name = entry.path().filename().string();
        item.full_path = entry.path().string();
        item.rel_path = std::filesystem::relative(entry.path(), proj, ec).generic_string();
        item.size_bytes = item.is_dir ? count_files_recursive(entry.path()) : entry.file_size(ec);
        item.category = item.is_dir ? "DIR" : entry.path().extension().string();
        if (!item.is_dir && !item.category.empty() && item.category[0] == '.') {
            item.category.erase(0, 1);
        }
        if (item.category.empty()) item.category = "FILE";
        st.assets.push_back(std::move(item));
    }

    std::sort(st.assets.begin(), st.assets.end(), [](const StudioAssetItem& a, const StudioAssetItem& b) {
        if (a.is_dir != b.is_dir) return a.is_dir > b.is_dir;
        return a.name < b.name;
    });
}

void studio_create_asset_subfolder(LauncherState& st, const std::string& raw_name) {
    if (st.active_project_dir.empty() || raw_name.empty()) return;
    std::string clean;
    for (char ch : raw_name) {
        if (std::isalnum(static_cast<unsigned char>(ch)) || ch == '_' || ch == '-') clean.push_back(ch);
    }
    if (clean.empty()) return;

    std::filesystem::path target = std::filesystem::path(st.active_project_dir) / studio_asset_category_root(st.asset_filter_idx);
    if (!st.asset_subpath.empty()) target /= st.asset_subpath;
    target /= clean;

    std::error_code ec;
    if (std::filesystem::create_directories(target, ec) || std::filesystem::is_directory(target, ec)) {
        studio_scan_assets(st);
        studio_log(st, "Created folder: " + clean);
    }
}

void studio_scan_mods(LauncherState& st) {
    st.mods.clear();
    if (st.active_project_dir.empty()) return;
    QvnModList list = {};
    if (qvn_mods_scan(st.active_project_dir.c_str(), &list)) {
        for (size_t i = 0; i < list.count; ++i) st.mods.push_back(list.items[i]);
        qvn_mods_free(&list);
    }
}

void studio_select_project(LauncherState& st, const std::string& name, const std::string& dir) {
    st.active_project_name = name;
    st.active_project_dir = dir;
    st.mod_scroll = 0;
    st.asset_subpath.clear();
    st.confirm_delete_idx = -1;
    std::snprintf(st.cfg.active_project, sizeof(st.cfg.active_project), "%s", name.c_str());
    studio_scan_mods(st);
    studio_scan_assets(st);
    studio_load_game_config(st);
}

void studio_scan_projects(LauncherState& st) {
    st.projects.clear();
    st.confirm_delete_idx = -1;
    QvnProjectList plist = {};
    if (qvn_projects_scan(st.proj_root.c_str(), &plist)) {
        for (size_t i = 0; i < plist.count; ++i) st.projects.push_back(plist.items[i]);
        qvn_projects_free(&plist);
    }
    if (st.projects.empty()) {
        st.active_project_dir.clear();
        st.active_project_name.clear();
        st.mods.clear();
        st.assets.clear();
        st.total_asset_files = 0;
        return;
    }
    bool found = false;
    for (const auto& p : st.projects) {
        if (st.active_project_dir == p.dir || (!st.active_project_name.empty() && st.active_project_name == p.name)) {
            st.active_project_dir = p.dir;
            st.active_project_name = p.name;
            found = true;
            break;
        }
    }
    if (!found) {
        st.active_project_name = st.projects[0].name;
        st.active_project_dir = st.projects[0].dir;
    }
    studio_scan_mods(st);
    studio_scan_assets(st);
    studio_load_game_config(st);
}

void studio_load_game_config(LauncherState& st) {
    st.game_cfg_loaded = false;
    st.game_cfg_dirty = false;
    qvn_config_default(&st.game_cfg);
    if (st.active_project_dir.empty()) return;
    std::filesystem::path p = std::filesystem::path(st.active_project_dir) / "config.json";
    if (qvn_config_load_file(p.string().c_str(), &st.game_cfg)) {
        st.game_cfg_loaded = true;
    }
    st.gcfg_title = st.game_cfg.window_title;
    st.gcfg_author = st.game_cfg.game_author;
    st.gcfg_entry = st.game_cfg.entry_script;
    st.gcfg_menu_title = st.game_cfg.menu_title;
    st.gcfg_menu_sub = st.game_cfg.menu_subtitle;
}

void studio_save_game_config(LauncherState& st) {
    if (st.active_project_dir.empty()) return;
    std::snprintf(st.game_cfg.window_title, sizeof(st.game_cfg.window_title), "%s", st.gcfg_title.c_str());
    std::snprintf(st.game_cfg.game_author, sizeof(st.game_cfg.game_author), "%s", st.gcfg_author.c_str());
    std::snprintf(st.game_cfg.entry_script, sizeof(st.game_cfg.entry_script), "%s", st.gcfg_entry.c_str());
    std::snprintf(st.game_cfg.menu_title, sizeof(st.game_cfg.menu_title), "%s", st.gcfg_menu_title.c_str());
    std::snprintf(st.game_cfg.menu_subtitle, sizeof(st.game_cfg.menu_subtitle), "%s", st.gcfg_menu_sub.c_str());
    std::filesystem::path p = std::filesystem::path(st.active_project_dir) / "config.json";
    if (qvn_config_save_file(p.string().c_str(), &st.game_cfg)) {
        st.game_cfg_dirty = false;
        studio_set_status(st, "Saved");
        studio_log(st, "Saved game config.json");
    } else {
        studio_log(st, "Error: Failed to save game config.json");
    }
}

void studio_clear_tmp_cache(LauncherState& st) {
    std::error_code ec;
    std::filesystem::remove_all("/tmp/quickvn_build", ec);
    studio_log(st, "Cleared /tmp/quickvn_build cache.");
    studio_set_status(st, "Cleaned");
}

void studio_create_project_now(LauncherState& st, const std::string& raw_name) {
    if (raw_name.empty()) return;
    std::string clean;
    for (char ch : raw_name) {
        if (std::isalnum(static_cast<unsigned char>(ch)) || ch == '_' || ch == '-') clean.push_back(ch);
    }
    if (clean.empty()) return;

    std::filesystem::path target = std::filesystem::path(st.proj_root) / clean;
    std::string font_src = platform_find_system_font("en");
    if (qvn_create_project(font_src.c_str(), target.string().c_str(), raw_name.c_str(), st.new_project_test_mod ? 1 : 0, static_cast<int>(st.default_template_buf))) {
        studio_scan_projects(st);
        studio_select_project(st, clean, target.string());
        studio_save_launcher_config(st);
        studio_set_status(st, "Created");
        studio_log(st, "Created project: " + clean);
    } else {
        studio_set_status(st, "Error");
        studio_log(st, "Error: Failed to create project " + clean);
    }
}

void studio_duplicate_project(LauncherState& st, const QvnProjectEntry& src) {
    std::error_code ec;
    std::string base = std::string(src.name) + "_copy";
    std::filesystem::path dst = std::filesystem::path(st.proj_root) / base;
    int suffix = 2;
    while (std::filesystem::exists(dst, ec)) {
        base = std::string(src.name) + "_copy" + std::to_string(suffix++);
        dst = std::filesystem::path(st.proj_root) / base;
    }
    std::filesystem::copy(src.dir, dst, std::filesystem::copy_options::recursive, ec);
    if (!ec) {
        studio_scan_projects(st);
        studio_select_project(st, base, dst.string());
        studio_log(st, "Duplicated -> " + base);
    } else {
        studio_log(st, "Error: Failed to duplicate " + std::string(src.name));
    }
}

void studio_validate_project(LauncherState& st) {
    if (st.active_project_dir.empty()) {
        studio_log(st, "Error: No active project.");
        return;
    }
    std::error_code ec;
    std::filesystem::path p(st.active_project_dir);
    int issues = 0;

    if (!std::filesystem::is_regular_file(p / "config.json", ec)) {
        studio_log(st, "Error: Missing config.json");
        issues++;
    } else {
        QvnConfig tmp = {};
        if (!qvn_config_load_file((p / "config.json").string().c_str(), &tmp)) {
            studio_log(st, "Error: Invalid config.json");
            issues++;
        } else {
            if (!std::filesystem::is_regular_file(p / tmp.entry_script, ec)) {
                studio_log(st, "Error: Missing script " + std::string(tmp.entry_script));
                issues++;
            }
            if (tmp.font_path[0] && !std::filesystem::is_regular_file(p / tmp.font_path, ec)) {
                studio_log(st, "Warning: Missing font " + std::string(tmp.font_path));
            }
        }
    }
    const char* dirs[] = {
        "assets/bg", "assets/chars", "assets/ui", "assets/fonts",
        "assets/audio/music", "assets/audio/sfx", "assets/audio/voice", "locales"
    };
    for (const char* d : dirs) {
        if (!std::filesystem::is_directory(p / d, ec)) {
            std::filesystem::create_directories(p / d, ec);
            studio_log(st, std::string("Created missing folder: ") + d);
        }
    }
    studio_scan_assets(st);
    if (issues == 0) {
        studio_log(st, "Validation passed: " + st.active_project_name);
        studio_set_status(st, "Valid");
    } else {
        studio_log(st, "Validation found " + std::to_string(issues) + " error(s).");
        studio_set_status(st, "Issues");
    }
}

void studio_create_rpy_starter(LauncherState& st) {
    if (st.active_project_dir.empty()) return;
    std::filesystem::path rpy_path = std::filesystem::path(st.active_project_dir) / "scripts" / "story.rpy";
    std::error_code ec;
    std::filesystem::create_directories(rpy_path.parent_path(), ec);
    if (!std::filesystem::is_regular_file(rpy_path, ec)) {
        std::ofstream out(rpy_path);
        out << "define a = Character(\"Alice\", color=\"#FACC15\")\n\n";
        out << "label start:\n";
        out << "    weather sakura\n";
        out << "    a \"Welcome to Ren'Py script mode in QuickVN!\"\n";
        out << "    ending \"Complete\" \"Transpiled cleanly\"\n";
        studio_log(st, "Created scripts/story.rpy");
    }
    platform_open_in_editor(rpy_path.string(), st.cfg.editor_command);
}

void studio_randomize_pak_key(LauncherState& st) {
    static const char chars[] = "abcdefghijklmnopqrstuvwxyz0123456789";
    std::string key = "qvn-";
    for (int i = 0; i < 24; ++i) {
        key.push_back(chars[std::rand() % (sizeof(chars) - 1)]);
    }
    st.pak_key_buf = key;
    std::snprintf(st.cfg.pak_key, sizeof(st.cfg.pak_key), "%s", key.c_str());
    studio_log(st, "Generated new PAK key.");
}

void studio_save_launcher_config(LauncherState& st) {
    std::snprintf(st.cfg.editor_command, sizeof(st.cfg.editor_command), "%s", st.editor_cmd_buf.c_str());
    std::snprintf(st.cfg.pak_key, sizeof(st.cfg.pak_key), "%s", st.pak_key_buf.c_str());
    st.cfg.studio_fps_limit = st.studio_fps_buf;
    st.cfg.auto_open_dist = st.auto_open_dist_buf ? 1 : 0;
    st.cfg.default_template = st.default_template_buf;
    const char* lcodes[3] = {"en", "ru", "ja"};
    std::snprintf(st.cfg.studio_lang, sizeof(st.cfg.studio_lang), "%s", lcodes[std::clamp(st.lang_idx, 0, 2)]);
    std::snprintf(st.cfg.active_project, sizeof(st.cfg.active_project), "%s", st.active_project_name.c_str());

    if (qvn_launcher_config_save(st.launcher_cfg_path.c_str(), &st.cfg)) {
        studio_set_status(st, "Saved");
        studio_log(st, "Saved studio.json");
    } else {
        studio_set_status(st, "Error");
        studio_log(st, "Error: Failed to save studio.json");
    }
}

void studio_save_mods(LauncherState& st) {
    if (st.active_project_dir.empty()) return;
    if (qvn_mods_save(st.active_project_dir.c_str(), st.mods.empty() ? nullptr : st.mods.data(), st.mods.size())) {
        studio_set_status(st, "Saved");
        studio_log(st, "Saved mods.json");
    } else {
        studio_set_status(st, "Error");
        studio_log(st, "Error: Failed to save mods.json");
    }
}

void studio_launch_game(LauncherState& st) {
    if (st.active_project_dir.empty()) {
        studio_set_status(st, "No Project");
        studio_log(st, "Error: Select or create a project first.");
        return;
    }
    std::string host = studio_host_platform_id();
    std::string configured;
    for (size_t i = 0; i < st.cfg.target_count && i < 5; ++i) {
        if (host == st.cfg.targets[i].id) {
            configured = st.cfg.targets[i].game_bin;
            break;
        }
    }
    std::string bin = studio_resolve_runtime_bin(st, host, configured);
    if (!bin.empty()) {
        std::error_code ec;
        std::string abs_proj = std::filesystem::weakly_canonical(st.active_project_dir, ec).string();
        if (abs_proj.empty()) abs_proj = st.active_project_dir;
        platform_spawn_detached("\"" + bin + "\" \"" + abs_proj + "\"");
        studio_set_status(st, "Running");
        studio_log(st, "Launched: " + st.active_project_name);
    } else {
        studio_set_status(st, "No Runtime");
        studio_log(st, "Error: Missing runtime stub for " + host);
    }
}

static void prepare_export_dirs(const std::filesystem::path& out_dir, const std::filesystem::path& proj_dir, bool is_mobile) {
    std::error_code ec;
    std::filesystem::create_directories(out_dir, ec);
    if (!is_mobile) {
        std::filesystem::create_directories(out_dir / "mods", ec);
        std::filesystem::create_directories(out_dir / "engine_mods", ec);
        std::filesystem::create_directories(out_dir / "saves", ec);
        if (std::filesystem::is_directory(proj_dir / "mods", ec)) {
            std::filesystem::copy(proj_dir / "mods", out_dir / "mods", std::filesystem::copy_options::recursive | std::filesystem::copy_options::overwrite_existing, ec);
        }
        if (std::filesystem::is_directory(proj_dir / "engine_mods", ec)) {
            std::filesystem::copy(proj_dir / "engine_mods", out_dir / "engine_mods", std::filesystem::copy_options::recursive | std::filesystem::copy_options::overwrite_existing, ec);
        }
        if (std::filesystem::is_regular_file(proj_dir / "mods.json", ec)) {
            std::filesystem::copy_file(proj_dir / "mods.json", out_dir / "mods.json", std::filesystem::copy_options::overwrite_existing, ec);
        }
    }
}

void studio_start_quick_pack(LauncherState& st) {
    if (st.building.load() || st.active_project_dir.empty()) return;
    if (st.worker.joinable()) st.worker.join();
    st.building.store(true);
    studio_set_status(st, "Packing...");

    std::string host = studio_host_platform_id();
    std::string configured;
    for (size_t i = 0; i < st.cfg.target_count && i < 5; ++i) {
        if (host == st.cfg.targets[i].id) {
            configured = st.cfg.targets[i].game_bin;
            break;
        }
    }
    std::string stub_bin = studio_resolve_runtime_bin(st, host, configured);
    std::string proj_dir = st.active_project_dir;
    std::string proj_name = st.active_project_name;
    std::string pak_key = st.cfg.pak_key;

    st.worker = std::thread([&st, host, stub_bin, proj_dir, proj_name, pak_key]() {
        if (stub_bin.empty()) {
            studio_log(st, "Error: Missing runtime stub for " + host);
            studio_set_status(st, "No Runtime");
            st.building.store(false);
            return;
        }
        std::filesystem::path out_dir = std::filesystem::path(proj_dir) / "dist" / host;
        std::string exe_name = proj_name;
#if defined(_WIN32)
        if (exe_name.size() < 4 || exe_name.substr(exe_name.size() - 4) != ".exe") exe_name += ".exe";
#endif
        std::filesystem::path out_exe = out_dir / exe_name;
        int ok = qvn_pack_executable(stub_bin.c_str(), out_exe.string().c_str(), proj_dir.c_str(), pak_key.c_str());
        if (ok) {
            prepare_export_dirs(out_dir, proj_dir, false);
            studio_log(st, "[" + host + "] Packed -> dist/" + host + "/" + exe_name);
            studio_set_status(st, "Packed");
            if (st.cfg.auto_open_dist) platform_open_folder(out_dir.string());
        } else {
            studio_log(st, "Error: Pack failed for " + host);
            studio_set_status(st, "Failed");
        }
        st.building.store(false);
    });
}

void studio_start_multi_build(LauncherState& st) {
    if (st.building.load() || st.active_project_dir.empty()) return;
    if (st.worker.joinable()) st.worker.join();
    st.building.store(true);
    studio_set_status(st, "Exporting...");

    QvnLauncherConfig cfg_copy = st.cfg;
    std::string self_dir = st.self_dir;
    std::string source_root = st.source_root;
    std::string runtimes_dir = st.runtimes_dir;
    std::string proj_dir = st.active_project_dir;
    std::string proj_name = st.active_project_name;
    bool do_pack = st.pack_single_file;

    st.worker = std::thread([&st, cfg_copy, self_dir, source_root, runtimes_dir, proj_dir, proj_name, do_pack]() {
        studio_log(st, "Export started: " + proj_name);
        bool all_ok = true;

        LauncherState resolver_ctx;
        resolver_ctx.self_dir = self_dir;
        resolver_ctx.runtimes_dir = runtimes_dir;

        for (size_t i = 0; i < cfg_copy.target_count && i < 5; ++i) {
            const QvnBuildTarget& t = cfg_copy.targets[i];
            if (!t.enabled) continue;

            std::string tid(t.id);
            bool is_mobile = (tid == "android" || tid == "ios");
            std::string runtime_bin = studio_resolve_runtime_bin(resolver_ctx, tid, t.game_bin);

            if (runtime_bin.empty()) {
                studio_log(st, "[" + tid + "] Missing runtime stub in runtimes/" + tid + "/");
                all_ok = false;
                continue;
            }

            std::filesystem::path out_dir = std::filesystem::path(proj_dir) / "dist" / tid;
            std::string out_name = proj_name;
            if (tid == "windows") out_name += ".exe";
            else if (tid == "android") out_name = "libquickjs_vn.so";

            std::filesystem::path out_bin = out_dir / out_name;
            std::error_code ec;
            std::filesystem::create_directories(out_dir, ec);

            if (do_pack && !is_mobile) {
                int packed = qvn_pack_executable(runtime_bin.c_str(), out_bin.string().c_str(), proj_dir.c_str(), cfg_copy.pak_key);
                if (packed) {
                    prepare_export_dirs(out_dir, proj_dir, false);
                    studio_log(st, "[" + tid + "] Packed -> dist/" + tid + "/" + out_name);
                } else {
                    studio_log(st, "[" + tid + "] Error: Encrypt failed.");
                    all_ok = false;
                }
            } else {
                std::filesystem::copy_file(runtime_bin, out_bin, std::filesystem::copy_options::overwrite_existing, ec);
                if (!ec) {
                    prepare_export_dirs(out_dir, proj_dir, is_mobile);
                    studio_log(st, "[" + tid + "] Exported -> dist/" + tid + "/" + out_name);
                } else {
                    studio_log(st, "[" + tid + "] Error: Copy failed.");
                    all_ok = false;
                }
            }
        }

        studio_log(st, all_ok ? "Export complete." : "Export finished with missing runtimes.");
        studio_set_status(st, all_ok ? "Done" : "Missing Stubs");
        st.building.store(false);
    });
}
