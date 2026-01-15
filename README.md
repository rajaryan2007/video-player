


SDL3.4 Video Player (Windows x64)

This project demonstrates video playback with audio using SDL 3.4 and FFmpeg on Windows x64.
It uses SDL3 GPU renderer for video and SDL3 audio streams for sound.

✅ Requirements

SDL 3.4 (x64)

You can download SDL 3.4
 or install via vcpkg:

vcpkg install sdl3:x64-windows


FFmpeg libraries (x64)
Required FFmpeg libs:

libavcodec

libavformat

libswresample

Install via vcpkg:

vcpkg install ffmpeg:x64-windows


CMake
Required to build the project.

⚙️ Setup in CMake

Example CMakeLists.txt for SDL3.4 + FFmpeg:
```
cmake_minimum_required(VERSION 3.25)
project(SDL3VideoPlayer)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_PREFIX_PATH "C:/vcpkg/installed/x64-windows")

find_package(SDL3 CONFIG REQUIRED)
find_package(SDL3main CONFIG REQUIRED)

find_package(PkgConfig REQUIRED)
pkg_check_modules(AVCODEC REQUIRED IMPORTED_TARGET libavcodec)
pkg_check_modules(AVFORMAT REQUIRED IMPORTED_TARGET libavformat)
pkg_check_modules(AVUTIL REQUIRED IMPORTED_TARGET libavutil)
pkg_check_modules(SWRESAMPLE REQUIRED IMPORTED_TARGET libswresample)

add_executable(SDL3VideoPlayer main.cpp)

target_link_libraries(SDL3VideoPlayer
    SDL3::SDL3
    SDL3main::SDL3main
    PkgConfig::AVCODEC
    PkgConfig::AVFORMAT
    PkgConfig::AVUTIL
    PkgConfig::SWRESAMPLE
)
```

Adjust CMAKE_PREFIX_PATH to your vcpkg install location.

🛠 Building
mkdir build
```
cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build .
```

Outputs SDL3VideoPlayer.exe

Make sure your FFmpeg DLLs (avcodec-*.dll, avformat-*.dll, etc.) are in the same folder as the executable or in PATH.

📁 Notes

This project is single-threaded. Audio/video sync is handled via audio clock using SDL_AudioStream queued bytes.

SDL3.4 does not provide direct playback position; you must track queued audio bytes manually.

Only SDL3.4 and FFmpeg are required. No other libraries.

Tested on Windows 10/11 x64.

⚡ Tips

Avoid opening SDL_AudioDevice inside the decode loop. Open it once at init.

Use SDL_PutAudioStreamData only when queued bytes < MAX_AUDIO_BUFFER. Otherwise, drop frame.

Compensate audio latency (~0.2s) for correct A/V sync.

This README assumes all dependencies are handled via vcpkg.
No external downloads other than SDL3.4 

