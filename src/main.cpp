#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#endif

#include <SDL.h>
#include <SDL_image.h>
#include <SDL_ttf.h>

#include <functional>

#include "coins/CoinManager.hpp"
#include "core/App.hpp"
#include "platform/Platform.hpp"
#include "ui/ConfigWindow.hpp"
#include "ui/CrucifixWindow.hpp"
#include "ui/IconBlockOverlay.hpp"
#include "ui/RansomWindow.hpp"
#include "ui/TauntWindow.hpp"
#include "ui/ThankYouWindow.hpp"

namespace {

// GUI subsystem has no console by default
void AttachCliConsole() {
#if defined(_WIN32)
    if (AllocConsole()) {
        FILE* dummy;
        freopen_s(&dummy, "CONOUT$", "w", stdout);
        freopen_s(&dummy, "CONOUT$", "w", stderr);
        freopen_s(&dummy, "CONIN$", "r", stdin);
    }
#endif
}

int RunDebugIcons() {
    AttachCliConsole();
    auto rects = rd::Platform::GetDesktopIconRects();
    std::cout << "Found " << rects.size() << " desktop icon rect(s):\n";
    for (const auto& r : rects) {
        std::cout << "  x=" << r.x << " y=" << r.y << " w=" << r.w << " h=" << r.h << "\n";
    }
    return 0;
}

// standalone UI component test harness
std::filesystem::path DebugAssetPath(const std::string& relative) {
    char* base = SDL_GetBasePath();
    std::filesystem::path result =
        (base ? std::filesystem::path(base) : std::filesystem::current_path()) / "assets" / relative;
    if (base) SDL_free(base);
    return result;
}

bool InitDebugSDL(bool needTTF) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::cout << "SDL_Init failed: " << SDL_GetError() << "\n";
        return false;
    }
    if (!(IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG)) {
        std::cout << "IMG_Init failed: " << IMG_GetError() << "\n";
        return false;
    }
    if (needTTF && TTF_Init() != 0) {
        std::cout << "TTF_Init failed: " << TTF_GetError() << "\n";
        return false;
    }
    return true;
}

void RunStandaloneLoop(std::uint32_t durationMs, const std::function<void(std::uint32_t, std::uint32_t)>& tick) {
    Uint32 start = SDL_GetTicks();
    Uint32 last = start;
    bool running = true;
    while (running && SDL_GetTicks() - start < durationMs) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = false;
        }
        Uint32 now = SDL_GetTicks();
        tick(now - start, now - last);
        last = now;
        SDL_Delay(16);
    }
}

int RunDebugWindow(const std::string& name) {
    AttachCliConsole();
    SDL_Rect bounds{};

    if (name == "iconblock") {
        if (!InitDebugSDL(/*needTTF=*/false)) return 1;
        SDL_GetDisplayBounds(0, &bounds);
        std::cout << "Spawning IconBlockOverlay standalone for 60s (or until closed)...\n"
                   << "Try Win+D on it now.\n";
        rd::IconBlockOverlay overlay(bounds.x, bounds.y, bounds.w, bounds.h,
                                      DebugAssetPath("images/stop_sign.png").string());
        RunStandaloneLoop(60000, [&](std::uint32_t, std::uint32_t) { overlay.Render(); });

    } else if (name == "taunt") {
        if (!InitDebugSDL(/*needTTF=*/false)) return 1;
        SDL_GetDisplayBounds(0, &bounds);
        std::cout << "Spawning a TauntWindow standalone (re-spawns every ~4-10s) for 60s...\n";
        std::vector<rd::TauntWindow> taunts;
        taunts.emplace_back(DebugAssetPath("images/idiot.png").string(), bounds.w, bounds.h);
        RunStandaloneLoop(60000, [&](std::uint32_t, std::uint32_t deltaMs) {
            taunts.erase(std::remove_if(taunts.begin(), taunts.end(), [](const rd::TauntWindow& t) { return t.Expired(); }),
                         taunts.end());
            if (taunts.empty()) taunts.emplace_back(DebugAssetPath("images/idiot.png").string(), bounds.w, bounds.h);
            for (auto& t : taunts) {
                t.Update(deltaMs);
                t.Render();
            }
        });

    } else if (name == "ransom") {
        if (!InitDebugSDL(/*needTTF=*/true)) return 1;
        SDL_GetDisplayBounds(0, &bounds);
        std::cout << "Spawning RansomWindow standalone for 60s (uses a throwaway config dir, "
                  << "won't touch real coin state)...\n";
        rd::CoinManager coins(std::filesystem::temp_directory_path() / "ransomdoors_debug");
        rd::RansomWindow ransomWin(bounds.w, bounds.h, coins, DebugAssetPath("images/ransom_idle.png").string(),
                                    DebugAssetPath("images/Gold.png").string(),
                                    DebugAssetPath("fonts/Cousine-Bold.ttf").string());
        RunStandaloneLoop(60000, [&](std::uint32_t elapsedMs, std::uint32_t deltaMs) {
            ransomWin.Update(deltaMs);
            std::uint32_t remaining = elapsedMs < 78000 ? 78000 - elapsedMs : 0;
            ransomWin.Render(remaining);
        });

    } else if (name == "thankyou") {
        if (!InitDebugSDL(/*needTTF=*/false)) return 1;
        SDL_GetDisplayBounds(0, &bounds);
        std::cout << "Spawning ThankYouWindow standalone for 60s (reveal choreography plays once, "
                     "then holds on the final frame - there's no phase timer to reset it out here)...\n";
        SDL_Point startPos{bounds.w / 4, bounds.h / 4}; // no real RansomWindow to grow from out here
        rd::ThankYouWindow thankYou(bounds.w, bounds.h, startPos, DebugAssetPath("images/ransom_idle.png").string(),
                                     DebugAssetPath("images/ok_sign.png").string(),
                                     DebugAssetPath("images/thx_txt.png").string());
        RunStandaloneLoop(60000, [&](std::uint32_t, std::uint32_t deltaMs) {
            thankYou.Update(deltaMs);
            thankYou.Render();
        });

    } else if (name == "config") {
        if (!InitDebugSDL(/*needTTF=*/false)) return 1;
        rd::Config cfg;
        rd::ConfigWindowAssets assets{DebugAssetPath("fonts/Cousine-Bold.ttf").string(),
                                       DebugAssetPath("images/Starlight.png").string()};
        std::cout << "Opening ConfigWindow modal - close it to exit...\n";
        rd::ConfigWindow::ShowModal(cfg, assets);

    } else if (name == "crucifix") {
        if (!InitDebugSDL(/*needTTF=*/false)) return 1;
        SDL_GetDisplayBounds(0, &bounds);
        rd::CrucifixWindow crucifix(bounds.w, bounds.h, DebugAssetPath("images/repent.gif").string());
        crucifix.Show();
        std::cout << "Spawning CrucifixWindow standalone, gif duration = " << crucifix.GifDurationMs()
                  << "ms, looping it for 60s...\n";
        RunStandaloneLoop(60000, [&](std::uint32_t, std::uint32_t deltaMs) {
            crucifix.Update(deltaMs);
            crucifix.Render();
        });

    } else {
        std::cout << "Unknown --debug-window '" << name
                  << "'. Known: iconblock, taunt, ransom, thankyou, crucifix.\n"
                  << "(The warning-icon beat isn't its own reusable class yet - it's inline in "
                  << "App::RenderWarning - so it's not covered here.)\n";
        return 1;
    }

    if (TTF_WasInit()) TTF_Quit();
    IMG_Quit();
    SDL_Quit();
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--debug-icons") == 0) return RunDebugIcons();
        if (std::strcmp(argv[i], "--debug-window") == 0) {
            if (i + 1 >= argc) {
                AttachCliConsole();
                std::cout << "Usage: --debug-window <iconblock|taunt|ransom|thankyou>\n";
                return 1;
            }
            return RunDebugWindow(argv[i + 1]);
        }
    }

    rd::Platform::KillOtherInstances(); // fresh launch wins over stale one

    rd::App app;
    return app.Run();
}
