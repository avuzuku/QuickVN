<div align="center">

[English](README.md) · [Русский](README.ru.md) · [日本語](README.ja.md)

# ⚡ QuickVN

[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg?style=flat-square&logo=c%2B%2B)](https://en.cppreference.com/w/cpp/17)
[![Rust](https://img.shields.io/badge/Rust-2021-orange.svg?style=flat-square&logo=rust)](https://www.rust-lang.org/)
[![SFML 3](https://img.shields.io/badge/SFML-3.0-green.svg?style=flat-square&logo=sfml)](https://www.sfml-dev.org/)
[![QuickJS](https://img.shields.io/badge/QuickJS-ES2023-yellow.svg?style=flat-square&logo=javascript)](https://bellard.org/quickjs/)
[![License: MIT](https://img.shields.io/badge/License-MIT-lightgrey.svg?style=flat-square)](LICENSE)

**QuickVN** is a native 2D visual novel engine and all-in-one development studio powered by **C++17**, **SFML 3**, **QuickJS**, and **Rust**. Write stories in modern JavaScript DSL or Ren'Py `.rpy` scripts, customize everything through `config.json` or the Studio UI, manage mods directly inside the game, and export encrypted builds for **5 platforms**.

</div>

---

## ✨ Features

- **QuickVN Studio (IDE & Packager)**:
  - Builds clean desktop bundles for **Linux**, **Windows**, and **macOS** (`out/<os>/QuickVN_Studio`).
  - All games live cleanly inside `proj/<GameName>/` with zero engine-source clutter.
  - **Workspace, Games, Assets Browser, Visual Config Editor, Mods, 5-OS Export, Settings, About**, and live **EN / RU / JA** UI switching.
- **Hybrid Core**:
  - **C++17 + SFML 3** — hardware-accelerated 2D rendering, camera pan/zoom, weather particles, audio mixing (BGM, SFX, Voice), and minimalist UI with cached text rendering.
  - **C11** — low-overhead bridge to the QuickJS runtime.
  - **Rust** — modular staticlib (`json`, `config`, `saves`, `mods`, `pak`, `rpy`, `projects`) with zero external crate dependencies and an embedded multilingual **M PLUS 1p** font (Latin, Cyrillic, Japanese).
- **In-Game Mod Manager**:
  - Built directly into every game (Main Menu & Pause Menu) — no external mod-manager binary needed.
  - Toggle `engine_mods/` and `mods/`, reorder load priority, generate a sample theme mod (`neon_violet`), and apply changes live without restarting.
- **5-Platform Export & ChaCha20 Packaging**:
  - Export for **Linux**, **Windows**, **macOS**, **Android**, and **iOS** directly from Studio (`runtimes/`).
  - Single-executable **ChaCha20** encrypted `.pak` packaging (`QVNPAK02`) with custom/randomized keys.
- **Desktop & Mobile Saves**:
  - **PC**: `saves/` folder with JSON state, choice history, `settings.json`, `profile.json`, and `240×135` PNG scene thumbnails.
  - **Mobile (Android / iOS)** or `"single_file_saves": true`: unified ChaCha20-encrypted `savedata.qvn` container storing all slots, settings, and profile in a single file.
- **Zero-Code `config.json` + Live Hot-Reload**:
  - Configure window, fonts, themes, dialogue box, NVL mode, choices, transitions, speaker dimming, UI sounds, keybindings, gallery, and achievements without touching C++ or Rust.
- **Dual Scripting (JS DSL + Ren'Py `.rpy`)**:
  - Write in concise JS DSL (`bg`, `char`, `say`, `choice`, `timedChoice`, `input`, `voice`, `weather`, `nvl`, `camZoom`, `shake`, `flash`, `chapter`, `ending`) or Ren'Py `.rpy` syntax (auto-transpiled by Rust).

---

## 📦 Prerequisites

### 🐧 Linux

**Arch Linux** (with `yay`):
```bash
sudo pacman -S --needed base-devel cmake sfml rust curl && yay -S --needed quickjs
```

**Ubuntu / Debian** (24.04+):
```bash
sudo apt update && sudo apt install -y build-essential cmake libsfml-dev curl git && curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y
```
*(If `quickjs` is not in apt: `git clone https://github.com/bellard/quickjs && cd quickjs && make && sudo make install`)*

### 🍎 macOS
```bash
brew install cmake sfml quickjs rust
```

### 🪟 Windows (MSYS2 UCRT64)
```bash
pacman -S --needed mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja mingw-w64-ucrt-x86_64-sfml mingw-w64-ucrt-x86_64-rust mingw-w64-ucrt-x86_64-quickjs
```

---

## 🚀 Building QuickVN Studio

### Local Build (Current OS)
All temporary CMake/Cargo artifacts are placed in `/tmp/quickvn_build` so the repository stays clean:
```bash
git clone https://github.com/avuzuku/QuickVN.git
cd QuickVN
chmod +x tools/build_studio.sh
./tools/build_studio.sh
```
Run Studio:
```bash
./out/linux/QuickVN_Studio
```

### Multi-OS CI Release (3 Desktop Studios + 5 Runtime Stubs)
Trigger `.github/workflows/release.yml` via **GitHub Actions → Run workflow** (or push a `v*` tag). It builds native `QuickVN_Studio` bundles for **Linux**, **Windows**, and **macOS**, and embeds all **5 runtime stubs** (`linux`, `windows`, `macos`, `android`, `ios`) inside each bundle's `runtimes/` directory.

---

## 📁 Project Workspace Structure

Inside any Studio bundle (`out/<os>/`):
```text
out/linux/
├── QuickVN_Studio
├── runtimes/                  # Prebuilt engine stubs for 5-OS export
│   ├── linux/
│   ├── windows/
│   ├── macos/
│   ├── android/
│   └── ios/
└── proj/                      # All your games live here
    └── MyNovel/
        ├── config.json
        ├── mods.json
        ├── assets/
        │   ├── bg/
        │   ├── chars/         # e.g. chars/alice/happy.png
        │   ├── ui/
        │   ├── fonts/         # Bundled M PLUS 1p font.ttf
        │   └── audio/
        │       ├── music/
        │       ├── sfx/
        │       └── voice/
        ├── scripts/           # main.js or main.rpy
        ├── locales/           # en.json, ru.json, ja.json
        ├── mods/              # Game content mods (patch.js)
        ├── engine_mods/       # Engine/UI mods (init.js + config.json)
        ├── saves/             # PC JSON+PNG saves or mobile savedata.qvn
        └── dist/              # Exported builds per platform
```

---

## 📜 Scripting Guide (`scripts/main.js` & `.rpy`)

### JavaScript DSL
```javascript
chapter({ en: "Prologue", ru: "Пролог", ja: "序章" }, "Morning", 2.0);

const alice = char({ en: "Alice", ru: "Алиса", ja: "アリス" }, { id: "alice", color: "#FACC15" });
// Automatically resolves assets/chars/alice/happy.png, assets/bg/school.png, assets/audio/music/main.ogg

music("main", true, 0.6);
bg("school", 0.5);
weather("sakura", 40);

input("What is your name?", "playerName", "Alex");
alice.happy("Good morning, {playerName}!", "center");
voice("alice/01");

nvl(true);
n("Fullscreen monologue (NVL) mode...");
nvl(false);

timedChoice(7.0, 0, {
    "Classroom": () => {
        achievement("study_hard");
        bg("classroom");
        ending("Classroom Route", "Clear!");
    },
    "Rooftop": () => {
        shake(12, 0.35);
        flash("#FFFFFF", 0.25);
        bg("rooftop");
        ending("Rooftop Route", "Clear!");
    }
});
```

### Ren'Py `.rpy` Support
Set `"entry_script": "scripts/main.rpy"` (or select the **Ren'Py .rpy** template in Studio):
```python
define a = Character("Alice", color="#FACC15")

label start:
    scene school
    show a happy at center
    play music main
    voice "alice/01"
    a "Welcome to Ren'Py syntax in QuickVN!"
    menu:
        "Go inside":
            a "Let's go!"
        "Stay outside":
            weather sakura
            a "The breeze feels nice."
    ending "The End" "Demo complete"
```

---

## 🎛 `config.json` Reference

| Section | Key Parameters |
|---|---|
| `meta` | `version`, `author` |
| `window` | `title`, `width`, `height`, `fps_limit`, `fullscreen`, `vsync` |
| `paths` | `font`, `font_ru`, `font_ja`, `entry_script`, `save_dir`, `default_save_slot` |
| `features` | `mods_enabled`, `gallery_enabled`, `achievements_enabled`, `dev_inspector_enabled`, `single_file_saves`, `save_slots_count`, `backlog_limit`, `skip_interval_ms` |
| `main_menu` | `enabled`, `title`, `subtitle`, `background`, `music`, `align`, `logo`, `show_continue`, `show_load`, `show_gallery`, `show_achievements`, `show_mods`, `show_settings`, `show_quit` |
| `theme` | `accent_color`, `accent_hover_color`, `panel_color`, `panel_border_color`, `button_color`, `button_hover_color`, `corner_radius`, `shadow_alpha` |
| `dialogue_box` | `x`, `y`, `width`, `height`, `color`, `outline_color`, `name_font_size`, `name_color`, `text_font_size`, `text_color`, `text_max_width`, `line_spacing` |
| `nvl` | `bg_color`, `x`, `y`, `max_width`, `max_lines` |
| `transitions` | `bg_fade_duration`, `char_fade_duration`, `char_move_duration`, `idle_breathing`, `vignette_strength`, `dim_inactive_chars`, `inactive_char_brightness` |
| `audio` / `ui_sounds` | `music_volume`, `sound_volume`, `voice_volume` / `choice`, `save`, `load`, `achievement` |
| `hot_reload` | `enabled`, `interval_ms` |
| `controls` | `advance`, `advance_alt`, `quicksave`, `quickload`, `reload`, `exit`, `backlog`, `auto_mode`, `skip_mode`, `hide_ui` |

---

## 🎮 Default Controls

| Input | Action |
|---|---|
| **Left Click / Tap / `Space` / `Enter`** | Skip typewriter or advance dialogue |
| **Right Click / `H`** | Hide / show UI |
| **`F5` / `F9`** | Quick Save / Quick Load |
| **`F6`** | Manual Hot-Reload (script + `config.json` + mods) |
| **`F10`** | Toggle Dev Inspector (FPS, command index, `engine.state`) |
| **`L` / `A` / `Tab`** | Backlog / Auto-Play / Skip |
| **`Escape`** | Pause Menu / Close overlay |

---

## 📜 License

Distributed under the [MIT](LICENSE) License. Copyright (c) 2026 **Aliaksandr Vazheika**.
