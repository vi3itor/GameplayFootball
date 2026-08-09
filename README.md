# Gameplay Football

Football game, a fork of discontinued [GameplayFootball](https://github.com/BazkieBumpercar/GameplayFootball) written by [Bastiaan Konings Schuiling](http://www.properlydecent.com/).

In 2019, Google Brain team picked up the game and created a Reinforcement Learning environment based on it - [Google Research Football](https://github.com/google-research/football). They made some improvements to the game, updated the libraries, but removed everything that was not necessary for their task, including menus and audio effects.

The goal of this repository is to update the existing code, based on Google Brain's changes and other forks, and make it compile and run on as many platforms as possible. PRs are welcome.

## Building from source

The project uses CMake. On Windows, dependencies are managed by vcpkg manifest mode through vcpkg.json. You do not need to run a separate classic-mode vcpkg install command.

### Linux

Install the required dependencies:

~~~bash
sudo apt-get install git cmake build-essential libgl1-mesa-dev libsdl2-dev \
libsdl2-image-dev libsdl2-ttf-dev libopenal-dev libboost-all-dev \
libdirectfb-dev libst-dev mesa-utils xvfb x11vnc python3-pip
~~~

Configure and build:

~~~bash
git clone https://github.com/vi3itor/GameplayFootball.git
cd GameplayFootball
cmake -S . -B build
cmake --build build --parallel
~~~

Run from the build directory so the relative asset paths resolve:

~~~bash
cd build
./gameplayfootball
~~~

### macOS (work in progress)

The project can compile on macOS, but rendering still requires additional work because the renderer currently expects to run on the main thread.

Install dependencies with [Homebrew](https://brew.sh/):

~~~bash
brew install git cmake sdl2 sdl2_image sdl2_ttf boost openal-soft
~~~

Configure and build:

~~~bash
git clone https://github.com/vi3itor/GameplayFootball.git
cd GameplayFootball
cmake -S . -B build
cmake --build build --parallel
~~~

### Windows

Install:

- [Visual Studio](https://visualstudio.microsoft.com/downloads/) with **Desktop development with C++** and CMake tools for Windows.
- [CMake](https://cmake.org/download/) 3.20 or newer, if it is not included in your Visual Studio installation.
- [Git](https://git-scm.com/download/win).

Visual Studio includes a vcpkg installation. A separate classic-mode vcpkg installation and vcpkg integrate install are not required. If you use another vcpkg installation, set VCPKG_ROOT to its root directory.

Clone the repository:

~~~powershell
git clone https://github.com/vi3itor/GameplayFootball.git
Set-Location GameplayFootball
~~~

Configure a 64-bit Visual Studio 2026 build. The manifest automatically restores the dependencies:

~~~powershell
$env:VCPKG_ROOT = 'C:\Program Files\Microsoft Visual Studio\18\Community\VC\vcpkg'
$toolchain = Join-Path $env:VCPKG_ROOT 'scripts\buildsystems\vcpkg.cmake'
cmake -S . -B out\build\x64-debug -G 'Visual Studio 18 2026' -A x64 "-DCMAKE_TOOLCHAIN_FILE=$toolchain" -DVCPKG_TARGET_TRIPLET=x64-windows
~~~

Build Debug:

~~~powershell
cmake --build out\build\x64-debug --config Debug --parallel 4
~~~

Build Release or RelWithDebInfo:

~~~powershell
cmake --build out\build\x64-debug --config Release --parallel 4
cmake --build out\build\x64-debug --config RelWithDebInfo --parallel 4
~~~

The build automatically copies the contents of data beside the executable. The output locations are:

~~~text
out\build\x64-debug\Debug\gameplayfootball.exe
out\build\x64-debug\Release\gameplayfootball.exe
out\build\x64-debug\RelWithDebInfo\gameplayfootball.exe
~~~

Run the game from its executable directory. This is also what launching the .exe from Windows Explorer does:

~~~powershell
Push-Location out\build\x64-debug\Debug
.\gameplayfootball.exe
Pop-Location
~~~

To run without a console window, configure with:

~~~powershell
cmake -S . -B out\build\x64-release -G 'Visual Studio 18 2026' -A x64 "-DCMAKE_TOOLCHAIN_FILE=$toolchain" -DVCPKG_TARGET_TRIPLET=x64-windows -DGAMEPLAYFOOTBALL_WINDOWS_SUBSYSTEM=ON
~~~

For debugging, leave GAMEPLAYFOOTBALL_WINDOWS_SUBSYSTEM disabled so startup errors remain visible in the terminal. Runtime diagnostics are written to log.txt beside the executable.

## Problems?

If you have problems, please open an issue with the CMake configure output, build configuration, and log.txt.

### Donate

If you want to thank Bastiaan for his great work, consider a donation to his Bitcoin address 1JHnTe2QQj8RL281fXFiyvK9igj2VhPh2t.
