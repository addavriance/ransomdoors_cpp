cmake_minimum_required(VERSION 3.20)
project(ransomdoors CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(SDL2 QUIET CONFIG)
find_package(SDL2_image QUIET CONFIG)
find_package(SDL2_mixer QUIET CONFIG)
find_package(SDL2_ttf QUIET CONFIG)

if(NOT TARGET SDL2::SDL2 OR NOT TARGET SDL2_image::SDL2_image OR NOT TARGET SDL2_mixer::SDL2_mixer OR NOT TARGET SDL2_ttf::SDL2_ttf)
    include(FetchContent)

    set(SDL_TEST OFF CACHE BOOL "" FORCE)
    set(SDL_TESTS OFF CACHE BOOL "" FORCE)
    set(BUILD_SHARED_LIBS ON CACHE BOOL "" FORCE)

    # We only ever load PNG/JPG textures and WAV audio - everything else pulls in
    # heavy third-party codec submodules (dav1d, libjxl, libvorbis, mpg123, ...)
    # that are unnecessary here and flaky to fetch over a plain git clone.
    set(SDL2IMAGE_TESTS OFF CACHE BOOL "" FORCE)
    set(SDL2IMAGE_AVIF OFF CACHE BOOL "" FORCE)
    set(SDL2IMAGE_JXL OFF CACHE BOOL "" FORCE)
    set(SDL2IMAGE_WEBP OFF CACHE BOOL "" FORCE)
    set(SDL2IMAGE_TIF OFF CACHE BOOL "" FORCE)
    set(SDL2IMAGE_JPG ON CACHE BOOL "" FORCE)
    set(SDL2IMAGE_PNG ON CACHE BOOL "" FORCE)

    set(SDL2MIXER_TESTS OFF CACHE BOOL "" FORCE)
    set(SDL2MIXER_MOD OFF CACHE BOOL "" FORCE)
    set(SDL2MIXER_MP3 OFF CACHE BOOL "" FORCE)
    set(SDL2MIXER_OGG OFF CACHE BOOL "" FORCE)
    set(SDL2MIXER_OPUS OFF CACHE BOOL "" FORCE)
    set(SDL2MIXER_FLAC OFF CACHE BOOL "" FORCE)
    set(SDL2MIXER_WAVPACK OFF CACHE BOOL "" FORCE)
    set(SDL2MIXER_MIDI OFF CACHE BOOL "" FORCE)

    # Only need Latin glyph shaping for our own hardcoded labels - HarfBuzz drags in
    # a whole extra build (and its own git clone) for nothing we use.
    set(SDL2TTF_HARFBUZZ OFF CACHE BOOL "" FORCE)

    FetchContent_Declare(SDL2 GIT_REPOSITORY https://github.com/libsdl-org/SDL.git GIT_TAG release-2.30.9 GIT_SHALLOW TRUE)
    FetchContent_Declare(SDL2_image GIT_REPOSITORY https://github.com/libsdl-org/SDL_image.git GIT_TAG release-2.8.2 GIT_SHALLOW TRUE)
    FetchContent_Declare(SDL2_mixer GIT_REPOSITORY https://github.com/libsdl-org/SDL_mixer.git GIT_TAG release-2.8.0 GIT_SHALLOW TRUE)
    FetchContent_Declare(SDL2_ttf GIT_REPOSITORY https://github.com/libsdl-org/SDL_ttf.git GIT_TAG release-2.24.0 GIT_SHALLOW TRUE)

    FetchContent_MakeAvailable(SDL2)
    FetchContent_MakeAvailable(SDL2_image)
    FetchContent_MakeAvailable(SDL2_mixer)
    FetchContent_MakeAvailable(SDL2_ttf)
endif()

# WIN32 puts this on the GUI subsystem (no console box popping up over the jumpscare
# overlay); the keyword is a no-op on non-Windows targets so it's safe unconditionally.
# rans0m.csproj: <ApplicationIcon>main_ico.ico</ApplicationIcon> - without this the exe
# just gets the generic default icon instead of the real one in Explorer/taskbar.
if(WIN32)
    set(RANSOMDOORS_RC resources/app.rc)
endif()

add_executable(ransomdoors WIN32
    ${RANSOMDOORS_RC}
    src/main.cpp
    src/core/App.cpp
    src/core/Sequencer.cpp
    src/audio/AudioManager.cpp
    src/coins/CoinManager.cpp
    src/consent/HardModeConsent.cpp
    src/ui/Window.cpp
    src/ui/Text.cpp
    src/ui/TauntWindow.cpp
    src/ui/RansomWindow.cpp
    src/ui/ThankYouWindow.cpp
    src/ui/IconBlockOverlay.cpp
    src/platform/Platform_win.cpp
    src/platform/Platform_mac.mm
    src/platform/TrayIcon_win.cpp
    src/platform/TrayIcon_mac.mm
)

target_include_directories(ransomdoors PRIVATE src)

if(APPLE)
    set_source_files_properties(src/platform/Platform_mac.mm src/platform/TrayIcon_mac.mm
        PROPERTIES COMPILE_FLAGS "-x objective-c++")
    target_link_libraries(ransomdoors PRIVATE "-framework Foundation")
    set_source_files_properties(src/platform/Platform_win.cpp src/platform/TrayIcon_win.cpp
        PROPERTIES HEADER_FILE_ONLY TRUE)
else()
    set_source_files_properties(src/platform/Platform_mac.mm src/platform/TrayIcon_mac.mm
        PROPERTIES HEADER_FILE_ONLY TRUE)
endif()

if(WIN32)
    target_link_libraries(ransomdoors PRIVATE advapi32 shell32 dwmapi)
endif()

target_link_libraries(ransomdoors PRIVATE
    SDL2::SDL2 SDL2::SDL2main
    SDL2_image::SDL2_image
    SDL2_mixer::SDL2_mixer
    SDL2_ttf::SDL2_ttf
)

add_custom_command(TARGET ransomdoors POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:ransomdoors>/assets
)

if(WIN32)
    add_custom_command(TARGET ransomdoors POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL2::SDL2>
            $<TARGET_FILE:SDL2_image::SDL2_image>
            $<TARGET_FILE:SDL2_mixer::SDL2_mixer>
            $<TARGET_FILE:SDL2_ttf::SDL2_ttf>
            $<TARGET_FILE_DIR:ransomdoors>
    )
endif()
