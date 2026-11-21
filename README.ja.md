<div align="center">

[English](README.md) · [Русский](README.ru.md) · [日本語](README.ja.md)

# ⚡ QuickVN

[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg?style=flat-square&logo=c%2B%2B)](https://en.cppreference.com/w/cpp/17)
[![Rust](https://img.shields.io/badge/Rust-2021-orange.svg?style=flat-square&logo=rust)](https://www.rust-lang.org/)
[![SFML 3](https://img.shields.io/badge/SFML-3.0-green.svg?style=flat-square&logo=sfml)](https://www.sfml-dev.org/)
[![QuickJS](https://img.shields.io/badge/QuickJS-ES2023-yellow.svg?style=flat-square&logo=javascript)](https://bellard.org/quickjs/)
[![License: MIT](https://img.shields.io/badge/License-MIT-lightgrey.svg?style=flat-square)](LICENSE)
[![Release](https://img.shields.io/github/v/release/avuzuku/QuickVN?style=flat-square&label=Release)](https://github.com/avuzuku/QuickVN/releases/latest)

**QuickVN** は **C++17**・**SFML 3**・**QuickJS**・**Rust** で構築された軽量・高速なネイティブ2Dビジュアルノベルエンジン＆開発スタジオです。モダンな JavaScript DSL または Ren'Py `.rpy` 形式でシナリオを記述し、`config.json` やスタジオGUIからすべてを設定し、ゲーム内蔵のMODマネージャーと **5OS向け暗号化エクスポート** を利用できます。

</div>

---

## ✨ 主な機能

- **QuickVN Studio（統合開発スタジオ）**:
  - **Linux**・**Windows**・**macOS** 向けのクリーンなスタジオバンドル（`out/<os>/QuickVN_Studio`）を生成。
  - すべてのゲームは `proj/<ゲーム名>/` 内に整理され、エンジンのソースコードに触れる必要はありません。
  - **ワークスペース・ゲーム一覧・素材ブラウザ・ビジュアル設定エディタ・MOD管理・5OS出力・スタジオ設定・情報**タブと、右上の **EN / RU / JA** 言語切替を搭載。
- **ハイブリッドコア**:
  - **C++17 + SFML 3** — ハードウェアアクセラレーション2D描画、カメラズーム/パン、天候パーティクル、オーディオ再生（BGM/SE/ボイス）、テキストキャッシュによる60FPS高速描画。
  - **C11** — QuickJS ランタイムへの低オーバーヘッドブリッジ。
  - **Rust** — 外部依存ゼロのモジュール化コア（`json`, `config`, `saves`, `mods`, `pak`, `rpy`, `projects`）と、多言語対応の **M PLUS 1p** フォント（英/露/日）をバイナリ内蔵。
- **ゲーム内蔵MODマネージャー**:
  - メインメニューおよびポーズメニューから直接起動（外部ツール不要）。
  - `engine_mods/` と `mods/` の有効化・読み込み順変更・テストMOD（`neon_violet` / `story_patch`）作成・再起動なしの即時リロードに対応。
- **5プラットフォーム出力＆ChaCha20暗号化パック**:
  - **Linux / Windows / macOS / Android / iOS** 向けにワンクリック出力（`runtimes/`）。
  - カスタム/ランダムキーによる単一実行ファイル暗号化（`QVNPAK02`）。
- **PC＆モバイル向けセーブ方式**:
  - **PC**: `saves/` フォルダにJSONセーブ、選択肢履歴、`settings.json`、`profile.json`、`240×135` PNGサムネイルを保存。
  - **モバイル (Android / iOS)** または `"single_file_saves": true`: すべてのスロット・設定・実績を1つの暗号化コンテナ `savedata.qvn` に保存。
- **フル設定可能な `config.json` + ホットリロード**:
  - ウィンドウ、フォント、テーマ、ダイアログ枠、NVLモード、メニュー項目、非話者の減光、UI効果音、キー配置、ギャラリー、実績をC++/Rust改変なしでカスタマイズ可能。
- **デュアルスクリプト対応（JS DSL + Ren'Py `.rpy`）**:
  - 簡潔な JS DSL と Rust による `.rpy` 自動トランスパイル、さらに `input()` 後の `{playerName}` 動的展開に対応。

---

## 📥 ダウンロード（リリース版）

ビルド済みの **QuickVN Studio** バンドル（**Linux / Windows / macOS / Android / iOS** 用のランタイムスタブ同梱）は [**Releases**](https://github.com/avuzuku/QuickVN/releases/latest) ページからダウンロードできます：

| OS | アーカイブ | 内容 |
|---|---|---|
| 🐧 **Linux** | `QuickVN_Studio_Linux.tar.gz` | `QuickVN_Studio` + 5OS対応 `runtimes/` + `proj/` |
| 🪟 **Windows** | `QuickVN_Studio_Windows.zip` | `QuickVN_Studio.exe` + 5OS対応 `runtimes/` + `proj/` |
| 🍎 **macOS** | `QuickVN_Studio_macOS.tar.gz` | `QuickVN_Studio` + 5OS対応 `runtimes/` + `proj/` |

> **Linux クイック起動:** `tar -xzf QuickVN_Studio_Linux.tar.gz && ./linux/QuickVN_Studio`

---

## 📦 必要な環境

### 🐧 Linux

**Arch Linux**（`yay` 使用）:
```bash
sudo pacman -S --needed base-devel cmake sfml rust curl && yay -S --needed quickjs
```

**Ubuntu / Debian** (24.04+):
```bash
sudo apt update && sudo apt install -y build-essential cmake libsfml-dev curl git && curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y
```
*(aptに `quickjs` がない場合: `git clone https://github.com/bellard/quickjs && cd quickjs && make && sudo make install`)*

### 🍎 macOS
```bash
brew install cmake sfml quickjs rust
```

### 🪟 Windows (MSYS2 UCRT64)
```bash
pacman -S --needed mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja mingw-w64-ucrt-x86_64-sfml mingw-w64-ucrt-x86_64-rust mingw-w64-ucrt-x86_64-quickjs
```

---

## 🚀 QuickVN Studio のビルド

### ローカルビルド（現在のOS用）
ビルド中間ファイルは `/tmp/quickvn_build` に生成されるため、リポジトリ内は常にクリーンに保たれます：
```bash
git clone https://github.com/avuzuku/QuickVN.git
cd QuickVN
chmod +x tools/build_studio.sh
./tools/build_studio.sh
```
起動：
```bash
./out/linux/QuickVN_Studio
```

---

## 📁 ワークスペース構造

Studioバンドル（`out/<os>/`）内部の構成：
```text
out/linux/
├── QuickVN_Studio
├── runtimes/                  # 5OS出力用のランタイムスタブ
│   ├── linux/
│   ├── windows/
│   ├── macos/
│   ├── android/
│   └── ios/
└── proj/                      # すべての制作プロジェクト
    └── MyNovel/
        ├── config.json
        ├── mods.json
        ├── assets/
        │   ├── bg/            # 背景画像
        │   ├── chars/         # 立ち絵（例: chars/alice/happy.png）
        │   ├── ui/            # UI素材
        │   ├── fonts/         # 内蔵 M PLUS 1p (font.ttf)
        │   └── audio/
        │       ├── music/     # BGM
        │       ├── sfx/       # 効果音
        │       └── voice/     # ボイス
        ├── scripts/           # main.js または main.rpy
        ├── locales/           # en.json, ru.json, ja.json
        ├── mods/              # ゲームコンテンツMOD (patch.js)
        ├── engine_mods/       # エンジン/UI MOD (init.js + config.json)
        ├── saves/             # セーブデータ + PNGプレビュー（または savedata.qvn）
        └── dist/              # プラットフォーム別ビルド出力
```

---

## 📜 スクリプトガイド (`scripts/main.js` & `.rpy`)

### JavaScript DSL
```javascript
chapter({ en: "Prologue", ru: "Пролог", ja: "序章" }, "朝の出会い", 2.0);

const alice = char({ en: "Alice", ru: "Алиса", ja: "アリス" }, { id: "alice", color: "#FACC15" });
// assets/chars/alice/happy.png や assets/bg/school.png などを自動解決

music("main", true, 0.6);
bg("school", 0.5);
weather("sakura", 40);

input("君の名前は？", "playerName", "アキラ");
alice.happy("おはよう、{playerName}！", "center");
voice("alice/01");

nvl(true);
n("全画面ノベル（NVL）モードのテスト……");
nvl(false);

timedChoice(7.0, 0, {
    "教室へ行く": () => {
        achievement("study_hard");
        bg("classroom");
        ending("教室ルート", "クリア！");
    },
    "屋上へ行く": () => {
        shake(12, 0.35);
        flash("#FFFFFF", 0.25);
        bg("rooftop");
        ending("屋上ルート", "クリア！");
    }
});
```

### Ren'Py (`.rpy`) 構文サポート
`config.json` で `"entry_script": "scripts/main.rpy"` を指定（または Studio で **Ren'Py .rpy** テンプレートを選択）：
```python
define a = Character("アリス", color="#FACC15")

label start:
    scene school
    show a happy at center
    play music main
    voice "alice/01"
    a "QuickVN の Ren'Py モードへようこそ！"
    menu:
        "校舎に入る":
            a "行こう！"
        "外に残る":
            weather sakura
            a "風がきもちいいね。"
    ending "終" "デモ完了"
```

---

## 🎛 `config.json` リファレンス

| セクション | 主な設定項目 |
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

## 🎮 デフォルト操作

| 操作 | 機能 |
|---|---|
| **左クリック / タップ / `Space` / `Enter`** | タイプライタースキップ / 次の文章へ進む |
| **右クリック / `H`** | UIの表示 / 非表示 |
| **`F5` / `F9`** | クイックセーブ / クイックロード |
| **`F6`** | 手動ホットリロード（スクリプト + `config.json` + MOD） |
| **`F10`** | 開発者インスペクタ切替（FPS・コマンド位置・`engine.state`） |
| **`L` / `A` / `Tab`** | バックログ / オート再生 / 早送り |
| **`Escape`** | ポーズメニュー / オーバーレイを閉じる |

---

## 📜 ライセンス

[MIT](LICENSE) ライセンスのもとで公開されています。Copyright (c) 2026 **Aliaksandr Vazheika**.
