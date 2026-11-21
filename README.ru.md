<div align="center">

[English](README.md) · [Русский](README.ru.md) · [日本語](README.ja.md)

# ⚡ QuickVN

[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg?style=flat-square&logo=c%2B%2B)](https://en.cppreference.com/w/cpp/17)
[![Rust](https://img.shields.io/badge/Rust-2021-orange.svg?style=flat-square&logo=rust)](https://www.rust-lang.org/)
[![SFML 3](https://img.shields.io/badge/SFML-3.0-green.svg?style=flat-square&logo=sfml)](https://www.sfml-dev.org/)
[![QuickJS](https://img.shields.io/badge/QuickJS-ES2023-yellow.svg?style=flat-square&logo=javascript)](https://bellard.org/quickjs/)
[![License: MIT](https://img.shields.io/badge/License-MIT-lightgrey.svg?style=flat-square)](LICENSE)

**QuickVN** — нативный 2D-движок визуальных новелл и студия разработки на базе **C++17**, **SFML 3**, **QuickJS** и **Rust**. Пишите сценарии на современном JS DSL или Ren'Py `.rpy`, настраивайте игру через `config.json` или визуальный редактор, управляйте модами прямо внутри игры и экспортируйте зашифрованные сборки под **5 платформ**.

</div>

---

## ✨ Возможности

- **QuickVN Studio (Студия и сборщик)**:
  - Генерирует чистые сборки для **Linux**, **Windows** и **macOS** (`out/<os>/QuickVN_Studio`).
  - Все проекты изолированы внутри `proj/<НазваниеИгры>/` — разработчик вообще не касается файлов движка.
  - Вкладки: **Обзор, Игры, Ассеты (с подпапками), Конфиг игры (с живым превью), Моды, Экспорт на 5 ОС, Настройки, О проекте** + переключение языка **EN / RU / JA** в шапке.
- **Гибридное ядро**:
  - **C++17 + SFML 3** — аппаратный рендеринг, камера (зум/панорамирование), погодные частицы, аудио-микшер (музыка, звуки, голос) и минималистичный UI с кэшированием текста (стабильные 60 FPS даже на CJK).
  - **C11** — прямой мост к QuickJS.
  - **Rust** — модульная библиотека (`json`, `config`, `saves`, `mods`, `pak`, `rpy`, `projects`) без внешних зависимостей и со встроенным мультиязычным шрифтом **M PLUS 1p** (латиница, кириллица, японский).
- **Встроенный менеджер модов**:
  - Находится **внутри самой игры** (в Главном меню и меню Паузы) — без лишних внешних программ.
  - Включение/отключение `engine_mods/` и `mods/`, изменение порядка загрузки, генератор тестового мода (`neon_violet` / `story_patch`) и горячее применение без перезапуска.
- **Экспорт под 5 платформ + шифрование ChaCha20**:
  - Экспорт для **Linux**, **Windows**, **macOS**, **Android** и **iOS** из Studio (`runtimes/`).
  - Упаковка в один зашифрованный исполняемый файл (`QVNPAK02`) со своим ключом.
- **Сохранения на ПК и мобильных**:
  - **ПК**: папка `saves/` с JSON-слотами, историей выборов, `settings.json`, `profile.json` и PNG-превью сцены (`240×135`).
  - **Мобильные (Android / iOS)** или `"single_file_saves": true`: единый зашифрованный контейнер `savedata.qvn`, хранящий все слоты, настройки и профиль в одном файле.
- **Полноценный `config.json` + Hot-Reload**:
  - Настройка окна, шрифтов, темы, диалогового окна, NVL-режима, кнопок меню, затемнения неактивных персонажей, звуков интерфейса, хоткеев, галереи и достижений без единой правки C++/Rust.
- **Два языка сценариев (JS DSL + Ren'Py `.rpy`)**:
  - Поддержка выразительного JS DSL и транспиляция скриптов Ren'Py `.rpy` на лету, а также динамическая подстановка `{playerName}` после `input()`.

---

## 📦 Установка зависимостей

### 🐧 Linux

**Arch Linux** (с `yay`):
```bash
sudo pacman -S --needed base-devel cmake sfml rust curl && yay -S --needed quickjs
```

**Ubuntu / Debian** (24.04+):
```bash
sudo apt update && sudo apt install -y build-essential cmake libsfml-dev curl git && curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y
```
*(Если `quickjs` отсутствует в apt: `git clone https://github.com/bellard/quickjs && cd quickjs && make && sudo make install`)*

### 🍎 macOS
```bash
brew install cmake sfml quickjs rust
```

### 🪟 Windows (MSYS2 UCRT64)
```bash
pacman -S --needed mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja mingw-w64-ucrt-x86_64-sfml mingw-w64-ucrt-x86_64-rust mingw-w64-ucrt-x86_64-quickjs
```

---

## 🚀 Сборка QuickVN Studio

### Локальная сборка (под текущую ОС)
Временные файлы сборки уходят в `/tmp/quickvn_build`, не засоряя проект:
```bash
git clone https://github.com/avuzuku/QuickVN.git
cd QuickVN
chmod +x tools/build_studio.sh
./tools/build_studio.sh
```
Запуск студии:
```bash
./out/linux/QuickVN_Studio
```

### Сборка всех 3 ОС и 5 рантаймов через GitHub Actions
Запустите workflow `.github/workflows/release.yml` во вкладке **Actions → Run workflow** (или отправьте тег `v*`). CI соберёт готовые архивы Studio для **Linux**, **Windows** и **macOS**, сразу укомплектованные стабами под все **5 платформ** в `runtimes/`.

---

## 📁 Структура рабочей директории

Внутри готовой папки Studio (`out/<os>/`):
```text
out/linux/
├── QuickVN_Studio
├── runtimes/                  # Рантайм-стабы для экспорта на 5 ОС
│   ├── linux/
│   ├── windows/
│   ├── macos/
│   ├── android/
│   └── ios/
└── proj/                      # Все игры разрабатываются здесь
    └── MyNovel/
        ├── config.json
        ├── mods.json
        ├── assets/
        │   ├── bg/            # Фоны
        │   ├── chars/         # Персонажи (напр. chars/alice/happy.png)
        │   ├── ui/            # Элементы интерфейса
        │   ├── fonts/         # Встроенный M PLUS 1p (font.ttf)
        │   └── audio/
        │       ├── music/     # Фоновая музыка
        │       ├── sfx/       # Звуковые эффекты
        │       └── voice/     # Озвучка персонажей
        ├── scripts/           # main.js или main.rpy
        ├── locales/           # en.json, ru.json, ja.json
        ├── mods/              # Контентные моды игры (patch.js)
        ├── engine_mods/       # Моды движка/UI (init.js + config.json)
        ├── saves/             # Сейвы + PNG превью (или savedata.qvn)
        └── dist/              # Готовые сборки по платформам
```

---

## 📜 Скриптинг (`scripts/main.js` и `.rpy`)

### JavaScript DSL
```javascript
chapter({ en: "Prologue", ru: "Пролог", ja: "序章" }, "Утро", 2.0);

const alice = char({ en: "Alice", ru: "Алиса", ja: "アリス" }, { id: "alice", color: "#FACC15" });
// Короткие имена автоматически ищутся в assets/chars/alice/, assets/bg/, assets/audio/...

music("main", true, 0.6);
bg("school", 0.5);
weather("sakura", 40);

input("Как тебя зовут?", "playerName", "Алекс");
alice.happy("Доброе утро, {playerName}!", "center");
voice("alice/01");

nvl(true);
n("Полноэкранный режим повествования (NVL)...");
nvl(false);

timedChoice(7.0, 0, {
    "В класс": () => {
        achievement("study_hard");
        bg("classroom");
        ending("Ветвь класса", "Финал");
    },
    "На крышу": () => {
        shake(12, 0.35);
        flash("#FFFFFF", 0.25);
        bg("rooftop");
        ending("Ветвь крыши", "Финал");
    }
});
```

### Поддержка Ren'Py (`.rpy`)
Укажите `"entry_script": "scripts/main.rpy"` или выберите шаблон **Ren'Py .rpy** при создании игры:
```python
define a = Character("Алиса", color="#FACC15")

label start:
    scene school
    show a happy at center
    play music main
    voice "alice/01"
    a "Привет из Ren'Py-скрипта внутри QuickVN!"
    menu:
        "Зайти внутрь":
            a "Пошли!"
        "Остаться во дворе":
            weather sakura
            a "Какой свежий ветер."
    ending "Конец" "Демо завершено"
```

---

## 🎛 Конфигурация `config.json`

| Секция | Основные параметры |
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

## 🎮 Управление по умолчанию

| Клавиша / Действие | Эффект |
|---|---|
| **ЛКМ / Тап / `Space` / `Enter`** | Пропустить печать текста или перейти к следующей реплике |
| **ПКМ / `H`** | Скрыть / показать интерфейс |
| **`F5` / `F9`** | Быстрое сохранение / Быстрая загрузка |
| **`F6`** | Ручной Hot-Reload (скрипт + `config.json` + моды) |
| **`F10`** | Инспектор разработчика (FPS, индекс команды, `engine.state`) |
| **`L` / `A` / `Tab`** | История диалогов / Авто-режим / Пропуск |
| **`Escape`** | Меню паузы / Закрыть окно |

---

## 📜 Лицензия

Распространяется по лицензии [MIT](LICENSE). Copyright (c) 2026 **Aliaksandr Vazheika**.
