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
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

    # only ever load PNG/JPG + WAV - other codec submodules are unnecessary and flaky to fetch
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

    # only need Latin glyph shaping for our hardcoded labels - HarfBuzz is unnecessary
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

# single header, negligible build cost, far less risk than hand-rolling a JSON parser
find_package(nlohmann_json QUIET CONFIG)
if(NOT TARGET nlohmann_json::nlohmann_json)
    include(FetchContent)
    FetchContent_Declare(nlohmann_json GIT_REPOSITORY https://github.com/nlohmann/json.git GIT_TAG v3.11.3 GIT_SHALLOW TRUE)
    set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(nlohmann_json)
endif()

# Dear ImGui has no CMakeLists.txt of its own - fetch sources only, build our own static lib target
include(FetchContent)
FetchContent_Declare(imgui GIT_REPOSITORY https://github.com/ocornut/imgui.git GIT_TAG v1.91.0 GIT_SHALLOW TRUE)
FetchContent_GetProperties(imgui)
if(NOT imgui_POPULATED)
    FetchContent_Populate(imgui)
endif()
add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl2.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdlrenderer2.cpp
)
target_include_directories(imgui PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
target_link_libraries(imgui PUBLIC SDL2::SDL2)

# WIN32 puts this on the GUI subsystem (no console box popping up); no-op on non-Windows targets
if(WIN32)
    set(RANSOMDOORS_RC resources/app.rc)

    file(GLOB_RECURSE RANSOMDOORS_ASSET_FILES CONFIGURE_DEPENDS RELATIVE ${CMAKE_SOURCE_DIR}/assets ${CMAKE_SOURCE_DIR}/assets/*)
    set(RANSOMDOORS_ASSETS_RC ${CMAKE_BINARY_DIR}/generated_assets.rc)
    set(RANSOMDOORS_ASSETS_RC_CONTENT "")
    foreach(RANSOMDOORS_ASSET_FILE ${RANSOMDOORS_ASSET_FILES})
        string(REPLACE "\\" "/" RANSOMDOORS_ASSET_KEY "${RANSOMDOORS_ASSET_FILE}")
        string(APPEND RANSOMDOORS_ASSETS_RC_CONTENT
            "\"${RANSOMDOORS_ASSET_KEY}\" ASSET \"${CMAKE_SOURCE_DIR}/assets/${RANSOMDOORS_ASSET_FILE}\"\n")
    endforeach()
    file(WRITE ${RANSOMDOORS_ASSETS_RC} "${RANSOMDOORS_ASSETS_RC_CONTENT}")
    list(APPEND RANSOMDOORS_RC ${RANSOMDOORS_ASSETS_RC})
endif()

add_executable(ransomdoors WIN32
    ${RANSOMDOORS_RC}
    src/main.cpp
    src/core/App.cpp
    src/core/Sequencer.cpp
    src/audio/AudioManager.cpp
    src/coins/CoinManager.cpp
    src/config/Config.cpp
    src/ui/Window.cpp
    src/ui/Text.cpp
    src/ui/TauntWindow.cpp
    src/ui/RansomWindow.cpp
    src/ui/ThankYouWindow.cpp
    src/ui/CrucifixWindow.cpp
    src/ui/GifAnimation.cpp
    src/ui/IconBlockOverlay.cpp
    src/ui/ConfigWindow.cpp
    src/ui/VignetteWindow.cpp
    src/platform/Platform_win.cpp
    src/platform/Platform_mac.mm
    src/platform/TrayIcon_win.cpp
    src/platform/TrayIcon_mac.mm
    src/platform/EmbeddedAssets.cpp
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

# SDL2::SDL2 keeps working when built static (SDL's own CMake aliases it to
# SDL2-static), but SDL2_image/_mixer/_ttf don't - their static target only
# gets the -static suffix.
target_link_libraries(ransomdoors PRIVATE
    SDL2::SDL2 SDL2::SDL2main
    SDL2_image::SDL2_image-static
    SDL2_mixer::SDL2_mixer-static
    SDL2_ttf::SDL2_ttf-static
    nlohmann_json::nlohmann_json
    imgui
)

add_custom_command(TARGET ransomdoors POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:ransomdoors>/assets
)
