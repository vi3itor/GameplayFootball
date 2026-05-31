# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## About

Fork of the discontinued [GameplayFootball](https://github.com/BazkieBumpercar/GameplayFootball) C++ football game. The goal is to modernize and keep it building on Linux, macOS, and Windows. The `google_brain` branch contains Google Brain's RL-focused variant as a reference.

## Build

**Prerequisites (Linux):**
```bash
sudo apt-get install git cmake build-essential libgl1-mesa-dev libsdl2-dev \
libsdl2-image-dev libsdl2-ttf-dev libsdl2-gfx-dev libopenal-dev libboost-all-dev \
libdirectfb-dev libst-dev mesa-utils xvfb x11vnc libsqlite3-dev
```

**Prerequisites (macOS):**
```bash
brew install cmake sdl2 sdl2_image sdl2_ttf sdl2_gfx boost openal-soft
```

**Build steps (Linux/macOS):**
```bash
cp -R data/. build
cd build
cmake ..
make -j$(nproc)
./gameplayfootball
```

> macOS note: the game compiles but does not run yet — rendering must happen on the main thread.

**Debug build:**
```bash
cd build && cmake -DCMAKE_BUILD_TYPE=Debug .. && make -j$(nproc)
```

There is no test suite. Validation is done by running the game.

## Architecture

The engine ("Blunted2") is a component/entity system split into static libraries linked into the `gameplayfootball` executable. `CMakeLists.txt` + `sources.cmake` define the library structure.

**Engine libraries (`blunted2` aggregate):**

| Library | Path | Role |
|---|---|---|
| `baselib` | `src/base/` | Math, geometry, logging, properties |
| `frameworklib` | `src/framework/` | Task scheduling, worker threads |
| `scenelib` | `src/scene/` | Scene graph (2D/3D), scene objects/resources |
| `systemsgraphicslib` | `src/systems/graphics/` | OpenGL renderer, graphics objects/tasks |
| `systemsaudiolib` | `src/systems/audio/` | OpenAL audio |
| `managerslib` | `src/managers/` | Resource managers |
| `utilslib` / `gui2lib` | `src/utils/` | Utilities, GUI widgets |
| `loaderslib` | `src/loaders/` | Asset loading |
| `typeslib` | `src/types/` | Shared type definitions |

**Game libraries (link against `blunted2`):**

| Library | Path | Role |
|---|---|---|
| `gamelib` | `src/onthepitch/` | Match simulation: ball, players, AI, teams, referee |
| `hidlib` | `src/hid/` | Human input device handling |
| `menulib` | `src/menu/` | Game menus |
| `datalib` | `src/data/` | Data loading (configs, databases) |
| `leaguelib` | `src/league/` | League/tournament logic |

**Entry point:** `src/main.cpp` → `src/blunted.cpp` (`Blunted` class) → `src/gametask.cpp` (`GameTask`).

**Rendering:** `src/systems/graphics/rendering/opengl_renderer3d.cpp` implements `IRenderer3D`. Shaders live in `data/media/shaders/` (GLSL: `ambient.frag`, `lighting.frag`, `postprocess.frag`, `simple.frag`). The renderer is driven by `GraphicsSystem` / `GraphicsTask` via the scheduler in `src/systems/graphics/scheduler.cpp`.

**Game data:** `data/` must be copied into `build/` before running (CMake does not do this automatically). Databases are SQLite files under `data/databases/`.
