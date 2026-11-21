#include "core/engine.hpp"
#include <filesystem>
#include <iostream>

#if defined(__ANDROID__) || (defined(__APPLE__) && TARGET_OS_IPHONE)
#include <SFML/Main.hpp>
#endif

int main(int argc, char** argv) {
    std::string config_path = "config.json";
    if (argc > 1 && argv[1] && argv[1][0] != '\0') {
        std::filesystem::path arg_p(argv[1]);
        if (std::filesystem::is_directory(arg_p)) {
            std::filesystem::current_path(arg_p);
        } else {
            config_path = argv[1];
        }
    }

    EngineState engine;
    if (!engine_init(engine, config_path)) {
        std::cerr << "Application initialization failed." << std::endl;
        engine_shutdown(engine);
        return -1;
    }

    engine_run(engine);
    engine_shutdown(engine);
    return 0;
}
