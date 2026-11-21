#pragma once
#include <SFML/Graphics.hpp>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

bool platform_read_bytes(const std::string& path, std::vector<uint8_t>& out_bytes);
bool platform_read_base_bytes(const std::string& path, std::vector<uint8_t>& out_bytes);
bool platform_read_bytes_smart(const std::string& path, const char* kind, std::vector<uint8_t>& out_bytes);
bool platform_read_text_file(const std::string& path, std::string& out_content);
bool platform_read_base_text_file(const std::string& path, std::string& out_content);
std::string platform_resolve_asset_path(const std::string& relative_path);
std::string platform_resolve_save_dir(const std::string& configured_dir);
std::string platform_find_system_font(const std::string& lang);
sf::Keyboard::Key platform_parse_key(const char* name);
sf::View platform_compute_view(sf::Vector2u window_size, float logical_width, float logical_height, uint8_t scale_mode);
sf::Vector2f platform_map_pixel(const sf::RenderWindow& window, sf::Vector2i pixel);

void platform_spawn_detached(const std::string& command);
void platform_open_in_editor(const std::string& file_path, const std::string& custom_editor);
void platform_open_folder(const std::string& dir_path);
int platform_run_command_capture(const std::string& command, const std::function<void(const std::string&)>& on_line);