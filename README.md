# ⚡ QuickVN

**QuickVN** is a lightweight, high-performance 2D visual novel engine written in **C++17**, powered by **SFML 3** for hardware-accelerated rendering and embedded **QuickJS** for scripting.

Unlike browser-based wrappers (e.g., Electron, NW.js) or heavyweight general-purpose game engines, QuickVN runs natively, talks directly to OpenGL, and consumes minimal system resources.

---

## ✨ Features

- 🚀 **Ultra-lightweight Footprint**: Consumes only ~167 MB of RAM and ~5% of a single CPU core under active rendering.
- 📜 **Modern JavaScript Scripting**: Scenarios are authored in clean ES2020 JavaScript without requiring C++ recompilation.
- 🎨 **Modern Tech Stack**: Native C++17 utilizing modern **SFML 3** event models and memory semantics.
- 📦 **Self-Contained Builds**: Integrated CMake targets automatically sync `assets/` and `scripts/` directories adjacent to the output binary.
- ⚡ **Linear Execution Model**: A command-queue architecture allows scripts to run synchronously at startup, building an execution pipeline that the C++ loop handles frame-by-frame.

---

## 🛠 Prerequisites

### Arch Linux

Install the required compiler, build tools, SFML 3, and QuickJS:

```bash
# Core build tools and SFML 3
sudo pacman -S cmake make gcc sfml

# QuickJS library from AUR
yay -S quickjs
```

---

## 🚀 Building & Running

1. **Clone the repository:**
   ```bash
   git clone https://github.com/avuzuku/QuickVN.git
   cd QuickVN
   ```

2. **Configure and build:**
   ```bash
   mkdir build && cd build
   cmake ..
   make -j$(nproc)
   ```

3. **Run the game:**
   ```bash
   ./quickjs_vn
   ```

> **Note:** The executable can be launched directly from the `build/` directory because CMake automatically copies the required `assets/` and `scripts/` directories into it during the build step.

---

## 📁 Project Structure

```text
QuickVN/
├── CMakeLists.txt        # Build system configuration
├── LICENSE               # MIT License
├── README.md             # Project documentation
├── assets/               # Fonts, backgrounds, and character sprites
│   ├── font.ttf
│   ├── bg_school.png
│   ├── bg_room.png
│   └── character.png
├── scripts/              # Scenario scripts
│   └── main.js
└── src/                  # Engine source code
    ├── Engine.cpp
    ├── Engine.hpp
    └── main.cpp
```

---

## 📝 Scripting Guide

All scenario logic is written in `scripts/main.js`. The engine exposes a global `engine` object to the JavaScript runtime:

```javascript
// Display scene backgrounds and character sprites
engine.showBackground("assets/bg_school.png");
engine.showCharacter("assets/character.png");

// Dialogue line (speaker, text)
engine.say("Alice", "Hello! Welcome to QuickVN.");
engine.say("Alice", "This visual novel engine is built with C++ and QuickJS.");
engine.say("Alice", "All scripting is handled via embedded JavaScript.");

// Transition to another location
engine.showBackground("assets/bg_room.png");
engine.say("Alice", "Location change completed successfully!");
```

### Controls

- **Left Mouse Click / Space / Enter**: Advance to the next dialogue line or trigger the next action.
- **Escape / Close Window**: Exit the application cleanly.

---

## 🗺 Roadmap

- [ ] **SFML Audio Integration**: Background music (`sf::Music`) and sound effects (`sf::Sound`).
- [ ] **Interactive Choices**: Branching story choices using JavaScript callbacks.
- [ ] **Visual Transitions**: Sprite alpha fading and position tweening.
- [ ] **Typewriter Text Effect**: Character-by-character text appearance with skip support.
- [ ] **State Serialization**: Save/Load system leveraging `JSON.stringify` on the JS runtime.
- [ ] **Script Hot-Reload**: Live scenario reloading during runtime for rapid development.

---

## 📜 License

Distributed under the [MIT](LICENSE) License.
