#include "platform/platform.hpp"
#include "rust_core.h"
#include <SFML/System/FileInputStream.hpp>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>

#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif

static std::string to_lower_ascii(const char* input) {
    std::string out;
    if (!input) return out;
    for (const char* p = input; *p; ++p) {
        out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(*p))));
    }
    return out;
}

static bool read_bytes_disk_fallback(const std::string& path, std::vector<uint8_t>& out_bytes) {
    sf::FileInputStream stream;
    if (stream.open(path)) {
        auto size_opt = stream.getSize();
        if (size_opt && *size_opt > 0) {
            std::vector<uint8_t> buf(*size_opt);
            auto read_opt = stream.read(buf.data(), *size_opt);
            if (read_opt) {
                buf.resize(*read_opt);
                out_bytes = std::move(buf);
                return true;
            }
        } else if (size_opt && *size_opt == 0) {
            out_bytes.clear();
            return true;
        }
    }

    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return false;
    std::streamsize sz = file.tellg();
    file.seekg(0, std::ios::beg);
    if (sz <= 0) {
        out_bytes.clear();
        return true;
    }
    out_bytes.resize(static_cast<size_t>(sz));
    return static_cast<bool>(file.read(reinterpret_cast<char*>(out_bytes.data()), sz));
}

bool platform_read_bytes(const std::string& path, std::vector<uint8_t>& out_bytes) {
    QvnBytes vfs_data = {};
    if (qvn_vfs_read_file(path.c_str(), &vfs_data)) {
        out_bytes.assign(vfs_data.data, vfs_data.data + vfs_data.len);
        qvn_bytes_free(&vfs_data);
        return true;
    }
    return read_bytes_disk_fallback(path, out_bytes);
}

bool platform_read_base_bytes(const std::string& path, std::vector<uint8_t>& out_bytes) {
    QvnBytes vfs_data = {};
    if (qvn_vfs_read_base_file(path.c_str(), &vfs_data)) {
        out_bytes.assign(vfs_data.data, vfs_data.data + vfs_data.len);
        qvn_bytes_free(&vfs_data);
        return true;
    }
    return read_bytes_disk_fallback(path, out_bytes);
}

bool platform_read_bytes_smart(const std::string& path, const char* kind, std::vector<uint8_t>& out_bytes) {
    if (platform_read_bytes(platform_resolve_asset_path(path), out_bytes)) {
        return true;
    }
    if (path.empty()) return false;

    std::string k = kind ? std::string(kind) : "";
    std::vector<std::string> prefixes;
    std::vector<std::string> exts;

    if (k == "music") {
        prefixes = { "", "assets/audio/music/", "assets/audio/", "assets/" };
        exts = { "", ".wav", ".ogg", ".mp3", ".flac" };
    } else if (k == "sfx") {
        prefixes = { "", "assets/audio/sfx/", "assets/audio/", "assets/" };
        exts = { "", ".wav", ".ogg", ".mp3", ".flac" };
    } else if (k == "voice") {
        prefixes = { "", "assets/audio/voice/", "assets/audio/", "assets/" };
        exts = { "", ".wav", ".ogg", ".mp3", ".flac" };
    } else if (k == "audio") {
        prefixes = { "", "assets/audio/music/", "assets/audio/sfx/", "assets/audio/voice/", "assets/audio/", "assets/" };
        exts = { "", ".wav", ".ogg", ".mp3", ".flac" };
    } else if (k == "bg") {
        prefixes = { "", "assets/bg/", "assets/" };
        exts = { "", ".png", ".jpg", ".jpeg", ".webp", ".bmp" };
    } else if (k == "char") {
        prefixes = { "", "assets/chars/", "assets/" };
        exts = { "", ".png", ".jpg", ".jpeg", ".webp", ".bmp" };
    } else if (k == "image") {
        prefixes = { "", "assets/bg/", "assets/chars/", "assets/ui/", "assets/" };
        exts = { "", ".png", ".jpg", ".jpeg", ".webp", ".bmp" };
    } else if (k == "font") {
        prefixes = { "", "assets/fonts/", "assets/" };
        exts = { "", ".ttf", ".otf", ".ttc" };
    } else {
        prefixes = { "", "assets/" };
        exts = { "" };
    }

    for (const auto& pre : prefixes) {
        for (const auto& ext : exts) {
            std::string candidate = pre + path + ext;
            if (candidate == path) continue;
            if (platform_read_bytes(platform_resolve_asset_path(candidate), out_bytes)) {
                return true;
            }
        }
    }
    return false;
}

bool platform_read_text_file(const std::string& path, std::string& out_content) {
    std::vector<uint8_t> raw;
    if (!platform_read_bytes(path, raw)) return false;
    out_content.assign(reinterpret_cast<const char*>(raw.data()), raw.size());
    return true;
}

bool platform_read_base_text_file(const std::string& path, std::string& out_content) {
    std::vector<uint8_t> raw;
    if (!platform_read_base_bytes(path, raw)) return false;
    out_content.assign(reinterpret_cast<const char*>(raw.data()), raw.size());
    return true;
}

std::string platform_resolve_asset_path(const std::string& relative_path) {
#if defined(__ANDROID__)
    return relative_path;
#elif defined(__APPLE__) && TARGET_OS_IPHONE
    const char* home = std::getenv("HOME");
    if (!relative_path.empty() && relative_path[0] == '/') return relative_path;
    if (std::filesystem::exists(relative_path)) return relative_path;
    if (home) {
        std::filesystem::path candidate = std::filesystem::path(home) / relative_path;
        if (std::filesystem::exists(candidate)) return candidate.string();
    }
    return relative_path;
#else
    return relative_path;
#endif
}

std::string platform_resolve_save_dir(const std::string& configured_dir) {
    std::filesystem::path p(configured_dir);
    if (p.is_absolute()) return configured_dir;
#if defined(__ANDROID__)
    const char* internal = std::getenv("ANDROID_DATA");
    const char* files_dir = std::getenv("QUICKVN_STORAGE_DIR");
    if (files_dir && files_dir[0] != '\0') return (std::filesystem::path(files_dir) / p).string();
    if (internal && internal[0] != '\0') return (std::filesystem::path(internal) / "local" / "tmp" / "quickvn" / p).string();
    return configured_dir;
#elif defined(__APPLE__) && TARGET_OS_IPHONE
    const char* home = std::getenv("HOME");
    if (home && home[0] != '\0') return (std::filesystem::path(home) / "Documents" / p).string();
    return configured_dir;
#else
    return configured_dir;
#endif
}

std::string platform_find_system_font(const std::string& lang) {
    static std::string cached_ja;
    static bool ja_searched = false;

    if (lang == "ja") {
        if (ja_searched) return cached_ja;
        ja_searched = true;

        const char* ja_candidates[] = {
            "/usr/share/fonts/TTF/NotoSansJP-Regular.ttf",
            "/usr/share/fonts/OTF/NotoSansJP-Regular.otf",
            "/usr/share/fonts/noto-cjk/NotoSansCJK-Regular.ttc",
            "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
            "/usr/share/fonts/google-noto-cjk/NotoSansCJK-Regular.ttc",
            "/System/Library/Fonts/Hiragino Sans W3.ttc",
            "C:/Windows/Fonts/YuGothR.ttc",
            "C:/Windows/Fonts/meiryo.ttc",
            "C:/Windows/Fonts/msgothic.ttc"
        };
        for (const char* p : ja_candidates) {
            if (std::filesystem::is_regular_file(p)) {
                cached_ja = p;
                return cached_ja;
            }
        }
#if !defined(_WIN32) && !defined(__ANDROID__) && !(defined(__APPLE__) && TARGET_OS_IPHONE)
        FILE* pipe = popen("fc-match -f '%{file}' :lang=ja 2>/dev/null", "r");
        if (pipe) {
            char buf[512] = {0};
            if (std::fgets(buf, sizeof(buf), pipe) != nullptr) {
                std::string path(buf);
                while (!path.empty() && (path.back() == '\n' || path.back() == '\r')) path.pop_back();
                pclose(pipe);
                if (!path.empty() && std::filesystem::is_regular_file(path)) {
                    cached_ja = path;
                    return cached_ja;
                }
            } else {
                pclose(pipe);
            }
        }
#endif
    }

    const char* general[] = {
        "/usr/share/fonts/TTF/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/noto/NotoSans-Regular.ttf",
        "/System/Library/Fonts/Helvetica.ttc",
        "C:/Windows/Fonts/segoeui.ttf"
    };
    for (const char* p : general) {
        if (std::filesystem::is_regular_file(p)) return p;
    }
    return "";
}

sf::Keyboard::Key platform_parse_key(const char* name) {
    std::string k = to_lower_ascii(name);
    if (k == "space") return sf::Keyboard::Key::Space;
    if (k == "enter" || k == "return") return sf::Keyboard::Key::Enter;
    if (k == "escape" || k == "esc") return sf::Keyboard::Key::Escape;
    if (k == "tab") return sf::Keyboard::Key::Tab;
    if (k == "backspace") return sf::Keyboard::Key::Backspace;
    if (k == "up") return sf::Keyboard::Key::Up;
    if (k == "down") return sf::Keyboard::Key::Down;
    if (k == "left") return sf::Keyboard::Key::Left;
    if (k == "right") return sf::Keyboard::Key::Right;
    if (k == "f1") return sf::Keyboard::Key::F1;
    if (k == "f2") return sf::Keyboard::Key::F2;
    if (k == "f3") return sf::Keyboard::Key::F3;
    if (k == "f4") return sf::Keyboard::Key::F4;
    if (k == "f5") return sf::Keyboard::Key::F5;
    if (k == "f6") return sf::Keyboard::Key::F6;
    if (k == "f7") return sf::Keyboard::Key::F7;
    if (k == "f8") return sf::Keyboard::Key::F8;
    if (k == "f9") return sf::Keyboard::Key::F9;
    if (k == "f10") return sf::Keyboard::Key::F10;
    if (k == "f11") return sf::Keyboard::Key::F11;
    if (k == "f12") return sf::Keyboard::Key::F12;
    if (k.size() == 1 && k[0] >= 'a' && k[0] <= 'z') {
        return static_cast<sf::Keyboard::Key>(static_cast<int>(sf::Keyboard::Key::A) + (k[0] - 'a'));
    }
    if (k.size() == 1 && k[0] >= '0' && k[0] <= '9') {
        return static_cast<sf::Keyboard::Key>(static_cast<int>(sf::Keyboard::Key::Num0) + (k[0] - '0'));
    }
    return sf::Keyboard::Key::Unknown;
}

sf::View platform_compute_view(sf::Vector2u window_size, float logical_width, float logical_height, uint8_t scale_mode) {
    sf::View view(sf::FloatRect({0.f, 0.f}, {logical_width, logical_height}));
    if (window_size.x == 0 || window_size.y == 0 || scale_mode == 1) {
        view.setViewport(sf::FloatRect({0.f, 0.f}, {1.f, 1.f}));
        return view;
    }

    float window_ratio = static_cast<float>(window_size.x) / static_cast<float>(window_size.y);
    float view_ratio = logical_width / logical_height;
    float size_x = 1.f;
    float size_y = 1.f;
    float pos_x = 0.f;
    float pos_y = 0.f;

    if (window_ratio > view_ratio) {
        size_x = view_ratio / window_ratio;
        pos_x = (1.f - size_x) * 0.5f;
    } else {
        size_y = window_ratio / view_ratio;
        pos_y = (1.f - size_y) * 0.5f;
    }

    view.setViewport(sf::FloatRect({pos_x, pos_y}, {size_x, size_y}));
    return view;
}

sf::Vector2f platform_map_pixel(const sf::RenderWindow& window, sf::Vector2i pixel) {
    return window.mapPixelToCoords(pixel);
}

void platform_spawn_detached(const std::string& command) {
#if defined(_WIN32)
    std::string cmd = "start \"\" " + command;
    std::system(cmd.c_str());
#elif !defined(__ANDROID__) && !(defined(__APPLE__) && TARGET_OS_IPHONE)
    std::string cmd = command + " >/dev/null 2>&1 &";
    int rc = std::system(cmd.c_str());
    (void)rc;
#else
    (void)command;
#endif
}

void platform_open_in_editor(const std::string& file_path, const std::string& custom_editor) {
    if (!custom_editor.empty()) {
        platform_spawn_detached(custom_editor + " \"" + file_path + "\"");
        return;
    }
#if defined(_WIN32)
    platform_spawn_detached("\"" + file_path + "\"");
#elif defined(__APPLE__) && !TARGET_OS_IPHONE
    platform_spawn_detached("open \"" + file_path + "\"");
#elif !defined(__ANDROID__) && !defined(__APPLE__)
    const char* env_editor = std::getenv("VISUAL");
    if (!env_editor || env_editor[0] == '\0') env_editor = std::getenv("EDITOR");
    if (env_editor && env_editor[0] != '\0') {
        platform_spawn_detached(std::string(env_editor) + " \"" + file_path + "\"");
    } else {
        platform_spawn_detached("xdg-open \"" + file_path + "\"");
    }
#else
    (void)file_path;
#endif
}

void platform_open_folder(const std::string& dir_path) {
#if defined(_WIN32)
    platform_spawn_detached("explorer \"" + dir_path + "\"");
#elif defined(__APPLE__) && !TARGET_OS_IPHONE
    platform_spawn_detached("open \"" + dir_path + "\"");
#elif !defined(__ANDROID__) && !defined(__APPLE__)
    platform_spawn_detached("xdg-open \"" + dir_path + "\"");
#else
    (void)dir_path;
#endif
}

int platform_run_command_capture(const std::string& command, const std::function<void(const std::string&)>& on_line) {
#if defined(__ANDROID__) || (defined(__APPLE__) && TARGET_OS_IPHONE)
    (void)command;
    (void)on_line;
    return -1;
#else
    std::string full_cmd = command + " 2>&1";
#if defined(_WIN32)
    FILE* pipe = _popen(full_cmd.c_str(), "r");
#else
    FILE* pipe = popen(full_cmd.c_str(), "r");
#endif
    if (!pipe) return -1;
    char buffer[512];
    while (std::fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        std::string line(buffer);
        while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) line.pop_back();
        if (on_line) on_line(line);
    }
#if defined(_WIN32)
    return _pclose(pipe);
#else
    return pclose(pipe);
#endif
#endif
}