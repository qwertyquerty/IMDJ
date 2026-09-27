# IMDJ

A lightweight prototype DJ mixer made with ImGui, ImGuiFileDialog, GLFW, miniaudio, RtMidi, VST3, Bungee, TagLib, nlohmann/json, magic_enum, and stb.

## Build

Needs CMake 3.21+, a C++20 compiler, and Ninja. All dependencies are fetched at configure time. Presets exist for each platform and build type, e.g. `windows-debug`, `windows-release`, `linux-release`, `macos-debug`.

```
cmake --preset windows-release
cmake --build --preset windows-release
ctest --preset windows-release
cd res && ../build/windows-release/bin/imdj.exe
```

On Windows, run these from a Visual Studio Developer Command Prompt, since the presets use MSVC's `cl`.

On Linux, install the windowing, audio, and GL development packages first:

```
sudo apt-get install ninja-build pkg-config libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev \
    libxkbcommon-dev libwayland-dev wayland-protocols libgl1-mesa-dev libasound2-dev libjack-jackd2-dev
```

The app loads its fonts, languages, themes, and dancers from the folders next to its executable, falling back to the working directory. In development, the working directory should be set to `res/`.

`cpack --config build/<preset>/CPackConfig.cmake` packages a zip with the executable and the contents of `res/` beside it, which is what a release ships.

## Auto formatter

Install the pinned clang-format with `pip install -r requirements.txt`, then run `python scripts/format.py` before pushing.

## Languages

UI text lives in `res/lang/<code>.json`, one flat `"key": "text"` mapping per language with `std::format` replacement style. English is the fallback for any key a language leaves out.

To add a language, copy `en.json` to `<code>.json`, translate the values, and add the language's own name as `lang.<code>` in `en.json`. Keep the keys and `{}` placeholders as they are; `{0}`, `{1}` can reorder them.

All non-English translations were made with automatic translation software and are unchecked by native speakers. Community contributions for translations are welcome.

## Themes

Each file in `res/themes/` is a theme: a `name`, a `font` path under `res/`, a `font_size` in pixels, and a `colors` object of `#RRGGBB` or `#RRGGBBAA` values. The bundled themes list every color key. Anything a theme leaves out keeps its default.

## Layout

```
src/
  main.cpp      app mainloop and the dashboard layout
  app/          window and ImGui lifecycle, settings files, session state, data paths
  audio/        the engine, the DSP pieces and the track metadata format
  core/         JSON, base64, path and text helpers, crash guard, channel matrix
  library/      song scanning, tag reading, session presets
  midi/         controller bindings and MIDI interface
  ui/           panels, popups, themes
  vst/          VST3 scanning and hosting
tests/          doctest unit test suite
scripts/        helper python scripts
res/            contents of this folder are shipped next to the executable
  fonts/        the UI font
  lang/         UI text translations
  themes/       default themes
  dancers/      animated GIF sprites synced to music and shown on the decks
```

## Licenses

IMDJ is released under the MIT License, see `LICENSE`.

| Component | License |
|---|---|
| Dear ImGui | MIT |
| ImGuiFileDialog | MIT |
| GLFW | zlib |
| miniaudio | Public domain or MIT-0 |
| RtMidi | MIT-style (RtMidi license) |
| VST3 SDK | MIT |
| Bungee | MPL 2.0 |
| Eigen (via Bungee) | MPL 2.0 |
| PFFFT (via Bungee) | BSD-style (FFTPACK license) |
| TagLib | LGPL 2.1 or MPL 1.1, used under MPL 1.1 |
| utfcpp (via TagLib) | Boost Software License 1.0 |
| nlohmann/json | MIT |
| magic_enum | MIT |
| stb_image | Public domain or MIT |
| Firple font | SIL Open Font License 1.1, see `fonts/Firple-LICENSE.txt` |

> Note: If anyone can find the original artist of `res/dancers/cat1_2b.gif` please let me know as I would like to credit them.
