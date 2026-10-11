# ⚡ QuickVN

**QuickVN** is a lightweight, high-performance 2D visual novel engine written in **C++17**, powered by **SFML 3** for hardware-accelerated rendering and embedded **QuickJS** for scripting.

Unlike browser-based wrappers (e.g., Electron, NW.js) or heavyweight general-purpose game engines, QuickVN runs natively, talks directly to OpenGL, and consumes minimal system resources.

---

## ✨ Features

- 🚀 **Ultra-lightweight Footprint**: Consumes only ~187 MB of RAM and ~5% of a single CPU core under active rendering.
- 📜 **Modern JavaScript Scripting**: Scenarios are authored in clean ES2020 JavaScript without requiring C++ recompilation.
- 🔀 **Interactive Choices & Branching**: Dynamic on-screen choice buttons with JavaScript callback closures.
- 🎵 **Integrated Audio System**: Looping background music streaming (`sf::Music`) and concurrent sound effects (`sf::Sound`).
- 🧩 **Modular Architecture**: Decoupled engine design cleanly separated into `core`, `graphics`, `audio`, and `script` subsystems.
- 🎨 **Modern Tech Stack**: Native C++17 utilizing modern **SFML 3** event models and memory semantics.
- 📦 **Self-Contained Builds**: Integrated CMake targets automatically sync `assets/` and `scripts/` directories adjacent to the output binary.

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
├── CMakeLists.txt              # Build system configuration
├── LICENSE                     # MIT License
├── README.md                   # Project documentation
├── assets/                     # Fonts, audio, backgrounds, and sprites
│   ├── font.ttf
│   ├── music_main.wav
│   ├── bell.wav
│   ├── click.wav
│   ├── bg_school.png
│   ├── bg_classroom.png
│   ├── bg_rooftop.png
│   ├── alice_normal.png
│   ├── alice_happy.png
│   └── alice_surprised.png
├── scripts/                    # Scenario scripts
│   └── main.js
└── src/                        # Modular source code
    ├── audio/
    │   ├── AudioManager.hpp
    │   └── AudioManager.cpp
    ├── core/
    │   ├── Command.hpp
    │   ├── Engine.hpp
    │   └── Engine.cpp
    ├── graphics/
    │   ├── Scene.hpp
    │   └── Scene.cpp
    ├── script/
    │   ├── ScriptManager.hpp
    │   └── ScriptManager.cpp
    └── main.cpp
```

---

## 📝 Scripting Guide

All scenario logic is written in `scripts/main.js`. The engine exposes a global `engine` object to the JavaScript runtime:

```javascript
// Start background music (looping) and setup the scene
engine.playMusic("assets/music_main.wav", true);
engine.showBackground("assets/bg_school.png");
engine.showCharacter("assets/alice_normal.png");

// Play audio effects
engine.playSound("assets/bell.wav");

engine.say("Alice", "Good morning! The morning bell just rang.");
engine.say("Alice", "Where should we go right now?");

// Interactive branching choices
engine.showChoices([
    "Go inside the Classroom",
    "Sneak onto the Rooftop",
    "Skip class and head Home"
], (choice) => {
    engine.playSound("assets/click.wav");

    if (choice === 0) {
        engine.showBackground("assets/bg_classroom.png");
        engine.showCharacter("assets/alice_happy.png");
        engine.say("Alice", "Let's review our notes near the window.");
    } else if (choice === 1) {
        engine.showBackground("assets/bg_rooftop.png");
        engine.showCharacter("assets/alice_surprised.png");
        engine.say("Alice", "The rooftop?! The view here is incredible.");
    } else {
        engine.stopMusic();
        engine.showCharacter("assets/alice_surprised.png");
        engine.say("Alice", "Leaving school already? Alright, adventure time!");
    }

    engine.say("Alice", "Branch execution finished.");
});
```

### Controls

- **Left Mouse Click / Space / Enter**: Advance dialogue.
- **Mouse Hover & Click**: Highlight and select on-screen branching options.
- **Escape / Close Window**: Exit the application cleanly.

---

## 🗺 Roadmap

- [x] **SFML Audio Integration**: Background music streaming (`sf::Music`) and sound effects (`sf::Sound`).
- [x] **Interactive Choices**: Branching story choices using JavaScript callbacks.
- [ ] **Visual Transitions**: Sprite alpha fading and position tweening.
- [ ] **Typewriter Text Effect**: Character-by-character text appearance with skip support.
- [ ] **State Serialization**: Save/Load system leveraging `JSON.stringify` on the JS runtime.
- [ ] **Script Hot-Reload**: Live scenario reloading during runtime for rapid development.

---

## 📜 License

Distributed under the [MIT](LICENSE) License. Copyright (c) 2026 **avuzuku**.