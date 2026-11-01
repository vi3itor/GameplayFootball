# Gameplay Football

A C++ football game based on the discontinued
[Gameplay Football](https://github.com/BazkieBumpercar/GameplayFootball) by
[Bastiaan Konings Schuiling](http://www.properlydecent.com/).

This fork aims to modernize the original game while retaining its menus, audio,
and match simulation. It also draws on changes from
[Google Research Football](https://github.com/google-research/football), the
reinforcement learning environment derived from the game in 2019. The
`google_brain` branch contains related reference work.

## Build status

| Target | Verification |
| --- | --- |
| macOS ARM64 | Release build passed with vcpkg; startup initialized OpenGL 4.1 and audio. Full gameplay has not been verified. |
| macOS x64 | To do: build and runtime validation. |
| Linux | Not verified with the current vcpkg configuration. |
| Windows x64 | Local Visual Studio 2026 Release build passed; intro and quick-start match rendered with OpenGL and audio initialized. Keyboard/gamepad input and full gameplay remain unverified. |
| Windows ARM64 | To do: build and runtime validation. |

The macOS startup check emitted a texture warning. A successful build and context
initialization do not establish that every scene renders correctly.

## Requirements

- Git and CMake 3.21 or newer.
- A C++ compiler and platform build tools.
- [vcpkg](https://github.com/microsoft/vcpkg) to build third-party dependencies.

The root `vcpkg.json` declares SDL2, SDL2_image (including JPEG support), SDL2_ttf,
Boost components, OpenAL Soft, and SQLite. Its baseline pins dependency
versions. CMake installs these automatically when configured with the vcpkg
toolchain; a separate `vcpkg install` command is unnecessary.

OpenGL comes from the platform SDK or system development packages. On macOS,
third-party libraries are linked statically and OpenGL uses the system framework.
Homebrew dependency libraries are not required.

## macOS

### Set up the tools

Install Apple's Command Line Tools if needed:

```bash
xcode-select --install
```

Install CMake and make sure `cmake` is available in your shell. Then set up vcpkg
once, choosing any location for its checkout:

```bash
git clone https://github.com/microsoft/vcpkg.git "$HOME/vcpkg"
"$HOME/vcpkg/bootstrap-vcpkg.sh" -disableMetrics
export VCPKG_ROOT="$HOME/vcpkg"
```

If vcpkg is already installed, set `VCPKG_ROOT` to that checkout instead. Set this
variable again in each new terminal session, or add it to your shell profile.

### Build for ARM64

Run from the GameplayFootball repository root:

```bash
cmake --preset mac-arm64-vcpkg
cmake --build --preset mac-arm64-vcpkg --parallel "$(sysctl -n hw.logicalcpu)"
```

The preset creates a Release build in `build/mac-arm64-vcpkg` using `arm64-osx`.
The first configure takes longer because vcpkg downloads and builds dependencies.
Later builds reuse installed dependencies and vcpkg's binary cache.

### Run

Copy the game data into the build directory, then run from that directory:

```bash
cp -R data/. build/mac-arm64-vcpkg/
cd build/mac-arm64-vcpkg
./gameplayfootball
```

The working directory must contain `football.config`, `databases/`, and `media/`.
CMake also stages these assets beside the executable after linking. Copy them again after changing
files under `data/`; this also replaces the staged configuration file.

### Build for x64 (to do)

Use a separate directory and override the preset's triplet:

```bash
cmake --preset mac-arm64-vcpkg -B build/mac-x64-vcpkg -DVCPKG_TARGET_TRIPLET=x64-osx
cmake --build build/mac-x64-vcpkg --parallel "$(sysctl -n hw.logicalcpu)"
cp -R data/. build/mac-x64-vcpkg/
cd build/mac-x64-vcpkg
./gameplayfootball
```

macOS x64 builds and execution are to do.

### Compatibility notes

- macOS creates the window and pumps events on the main thread; OpenGL rendering runs on its renderer thread.
- OpenAL Soft is pinned to `1.23.1#2`. Version 1.25.2 failed to compile with the
  selected Apple Command Line Tools because their C++ library lacked
  `std::views::join`.
- The build still produces warnings from existing game code.

## Linux

The following is a starting point for Debian/Ubuntu; this path has not been
verified with the current manifest. Install the compiler, CMake, Git, and system
OpenGL development files:

```bash
sudo apt-get update
sudo apt-get install build-essential cmake git curl zip unzip tar pkg-config libgl1-mesa-dev
```

Bootstrap vcpkg as in the macOS setup above, set `VCPKG_ROOT`, then run from the
repository root:

```bash
cmake -S . -B build/linux-vcpkg \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
cmake --build build/linux-vcpkg --parallel "$(nproc)"
cp -R data/. build/linux-vcpkg/
cd build/linux-vcpkg
./gameplayfootball
```

SDL's window and audio backends may require additional system development
packages for your distribution and desktop environment.

## Windows

Windows x64 has passed a local Visual Studio 2026 Release build and a match rendering check.
Full input, audio playback, and shutdown validation are still needed. Install
Visual Studio 2022 with the **Desktop development with C++** workload, Git, and
CMake. Use a Visual Studio Developer
Command Prompt.

```bat
git clone https://github.com/microsoft/vcpkg.git C:\dev\vcpkg
call C:\dev\vcpkg\bootstrap-vcpkg.bat -disableMetrics
set VCPKG_ROOT=C:\dev\vcpkg
```

From the GameplayFootball repository root, configure the x64 target:

```bat
cmake --preset windows-x64-vcpkg
cmake --build --preset windows-x64-vcpkg --parallel
cd build\windows-x64-vcpkg\Release
gameplayfootball.exe
```

The default Windows executable keeps a console for diagnostics. To build the GUI
subsystem variant, configure with `-DGAMEPLAYFOOTBALL_WINDOWS_SUBSYSTEM=ON`.
Both variants link SDL2main. The Windows CI workflow builds both variants; a
passing compile does not replace a GPU, input, audio, and shutdown smoke test.

### Windows ARM64 (to do)

Add a separate preset and CI job, then validate the build and runtime on Windows
ARM64 hardware. This target is not enabled yet.

## Continuous integration

[macOS ARM64 build](.github/workflows/macos-build.yml) compiles the ARM64 Release
preset and checks the executable architecture and staged assets. It bootstraps
vcpkg at the manifest baseline and enforces the project's VLA compiler guard.
[Windows x64 build](.github/workflows/windows-build.yml) builds the Windows x64 console and
GUI variants. It also clones vcpkg and checks out the manifest baseline before
bootstrapping, so it does not depend on the runner's preinstalled vcpkg history.
Both workflows run on pull requests, pushes to `main` or `mac`,
and manual dispatch. These are build checks; gameplay still needs local testing.

## Portability and validation

C++ targets use standard C++14, and GCC/Clang builds reject variable-length arrays
with `-Werror=vla`. Keep dynamically sized storage in `std::vector` so MSVC can
compile the same source. The Windows replacements in loaders, animation,
renderer buffers, pitch generation, team AI, and GUI layout are retained.
macOS requests an OpenGL core profile; Windows and Linux retain a compatibility
profile for the remaining immediate-mode calls. SQLite is supplied by vcpkg on
all platforms, and surface resizing uses SDL2 without SDL2_gfx.

The ARM64 Release build passed with all 177 C++ compilation commands
using `-std=c++14` and `-Werror=vla`. This validates the compiled macOS paths;
Windows x64 also passed a local MSVC Release build; Windows ARM64 is still to do.

Before merging platform changes:

1. Build the Windows console and GUI variants using the
   [Windows build workflow](.github/workflows/windows-build.yml).
2. On Windows and macOS, launch from the staged executable directory and check
   menus, a match, keyboard/gamepad input, audio, and exit.
3. Inspect `log.txt` for the OpenGL adapter/version and rendering errors. Resolve
   or investigate the macOS texture warning before claiming visual validation.

The presets set `CMAKE_POLICY_VERSION_MINIMUM=3.5` for CMake 4 compatibility
with the pinned OpenAL package's older config file. When configuring without a
preset on CMake 4, also pass `-DCMAKE_POLICY_VERSION_MINIMUM=3.5`.

## Troubleshooting

- **CMake cannot find SDL2 or another package:** check `VCPKG_ROOT` and configure
  with the preset or explicit vcpkg toolchain shown above.
- **Changing toolchain or architecture:** use a fresh build directory. An existing
  CMake cache can retain paths and architecture settings from an earlier build.
- **Missing configuration, database, or media:** stage `data/` and run with the
  build directory as the working directory.
- **Dependency build failure:** inspect the log path printed by vcpkg. Include the
  failing package, compiler version, target triplet, and relevant errors in an issue.
- **Rendering or startup issue:** include `log.txt` from the run directory and
  terminal output, along with the OS and GPU details.

The match-report serializer has headless regression tests, runnable with CTest.
Game simulation and gameplay still require building and running the game;
distinguish serializer/startup checks from full gameplay testing.

## Match reports

Optional JSON match reports are available for PASS-style analysis. Reporting is
disabled by default; see [setup, schema, and attribution](FORK_BRANCH_INVENTORY.md#pass-match-reports).

## Source layout

| Path | Purpose |
| --- | --- |
| `src/main.cpp` | Application entry point and task setup |
| `src/base/`, `src/framework/`, `src/scene/`, `src/systems/` | Blunted2 engine and platform systems |
| `src/onthepitch/` | Match simulation, players, teams, and AI |
| `src/menu/`, `src/hid/` | Menus and input handling |
| `data/` | Configuration, databases, shaders, and other game assets |
| `CMakeLists.txt`, `sources.cmake` | Build targets and source lists |
| `vcpkg.json`, `CMakePresets.json` | Dependency versions and platform build presets |

## Contributing

Issues and pull requests are welcome. Include reproducible steps and state which
platforms you built and tested. See [LICENSE](LICENSE) for licensing information.

## Support the original author

Bastiaan's Bitcoin donation address: `1JHnTe2QQj8RL281fXFiyvK9igj2VhPh2t`.
