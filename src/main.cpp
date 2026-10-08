#include "Engine.hpp"
#include <iostream>

int main() {
    Engine engine;
    if (!engine.init()) {
        std::cerr << "Initialization failed." << std::endl;
        return -1;
    }
    engine.run();
    return 0;
}
