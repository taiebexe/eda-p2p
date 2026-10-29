# EDA P2P

Shared project skeleton for the three-person TH Köln EDA team.
Uses C++17, Qt 6 Quick/QML and CMake; requires Qt 6.5+, CMake 3.21+ and Ninja.
Currently opens an empty window. Qt Network is linked for future work.

## Qt Creator (macOS, Windows, Linux)

Open `CMakeLists.txt`, select your installed Qt 6 desktop kit and matching
compiler, configure with Ninja, then build the `eda-p2p` target.

## Command line

Adjust Qt version/kit paths to your installation. Run from the repo root.

### macOS

```sh
export PATH="$HOME/Qt/Tools/CMake/CMake.app/Contents/bin:$HOME/Qt/Tools/Ninja:$PATH"
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH="$HOME/Qt/6.12.0/macos"
cmake --build build
```

### Windows

Use Developer PowerShell for VS 2022 with the matching MSVC Qt kit:

```powershell
$env:Path = "C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;C:\Qt\6.12.0\msvc2022_64\bin;$env:Path"
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH="C:\Qt\6.12.0\msvc2022_64"
cmake --build build
```

### Linux

Use an installed GCC/Clang toolchain and the Qt desktop kit:

```sh
export PATH="$HOME/Qt/Tools/CMake/bin:$HOME/Qt/Tools/Ninja:$PATH"
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH="$HOME/Qt/6.12.0/gcc_64"
cmake --build build
```
