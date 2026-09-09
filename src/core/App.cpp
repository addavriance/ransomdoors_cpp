#include "App.hpp"

#include <SDL_image.h>
#include <SDL_ttf.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>

#include "../platform/ConfigDialog.hpp"
#include "../platform/Platform.hpp"

namespace rd {

namespace {

constexpr std::array<const char*, 8> kTauntImages = {
    "images/glitch.jpg",  "images/idiot.png",        "images/ransom_idle.png", "images/ransom_random.png",
    "images/stop_sign.png", "images/static1.png",    "images/taunt2.jpg",      "images/taunt3.jpeg",
};

std::filesystem::path PlatformConfigRoot() {
#if defined(_WIN32)
    if (const char* appData = std::getenv("APPDATA")) return std::filesystem::path(appData) / "ransomdoors";
    return std::filesystem::temp_directory_path() / "ransomdoors";
#elif defined(__APPLE__)
    if (const char* home = std::getenv("HOME")) {
        return std::filesystem::path(home) / "Library" / "Application Support" / "ransomdoors";
    }
    return std::filesystem::temp_directory_path() / "ransomdoors";
#else
    if (const char* home = std::getenv("HOME")) return std::filesystem::path(home) / ".config" / "ransomdoors";
    return std::filesystem::temp_directory_path() / "ransomdoors";
#endif
}

} // namespace

std::filesystem::path App::ConfigDir() const { return PlatformConfigRoot(); }

std::filesystem::path App::AssetPath(const std::string& relative) const {
    static std::filesystem::path base = [] {
        char* p = SDL_GetBasePath();
        std::filesystem::path result = p ? std::filesystem::path(p) : std::filesystem::current_path();
        if (p) SDL_free(p);
        return result / "assets";
    }();
    return base / relative;
}

bool App::Init() {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) return false;
    if (!(IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG) & (IMG_INIT_PNG | IMG_INIT_JPG))) return false;
    if (TTF_Init() != 0) return false;
    if (!audio_.Init()) return false;

    audio_.LoadSfx("spawn", AssetPath("sounds/spawn.wav").string());
    audio_.LoadSfx("install", AssetPath("sounds/install.wav").string());
    audio_.LoadSfx("cash", AssetPath("sounds/cash.wav").string());
    audio_.LoadSfx("attack", AssetPath("sounds/attack.wav").string());
    audio_.LoadSfx("tada", AssetPath("sounds/tada.wav").string());
    audio_.LoadMusic("layer1", AssetPath("sounds/layer1.wav").string());
    audio_.LoadMusic("layer2", AssetPath("sounds/layer2.wav").string());
    audio_.LoadMusic("layer3", AssetPath("sounds/layer3.wav").string());

    SDL_Rect bounds{};
    SDL_GetDisplayBounds(0, &bounds);
    screenW_ = bounds.w;
    screenH_ = bounds.h;

    overlay_ = std::make_unique<Window>("A-90", screenW_, screenH_,
                                         SDL_WINDOW_ALWAYS_ON_TOP | SDL_WINDOW_SKIP_TASKBAR,
                                         /*startVisible=*/false);
    if (!overlay_->Valid()) return false;
    overlay_->SetPosition(bounds.x, bounds.y);
    SDL_SetWindowOpacity(overlay_->Raw(), 0.0f);
    overlay_->Show(); // avoids launch flash

    texRansomIdle_ = overlay_->LoadTexture(AssetPath("images/ransom_idle.png").string());
    texStopSign_ = overlay_->LoadTexture(AssetPath("images/stop_sign.png").string());
    texRansomAttack_ = overlay_->LoadTexture(AssetPath("images/ransom_attack.png").string());
    texProgressBar_ = overlay_->LoadTexture(AssetPath("images/progress_bar.png").string());
    texProgressElem_ = overlay_->LoadTexture(AssetPath("images/progress_elem.png").string());

    text_ = std::make_unique<TextRenderer>(overlay_->Renderer());
    text_->LoadFont(AssetPath("fonts/Cousine-Bold.ttf").string());

    consent_ = std::make_unique<HardModeConsent>(ConfigDir());
    coins_ = std::make_unique<CoinManager>(ConfigDir());
    Platform::RegisterGoldIcon(AssetPath("images/Gold.ico"));
    Platform::InstallKeyboardHook([this] { globalKeyPressed_ = true; });

    bool firstRun = !std::filesystem::exists(ConfigDir() / "config.json");
    config_.Load(ConfigDir());
    if (firstRun) {
        if (Platform::ShowConfigDialog(config_)) config_.Save(ConfigDir());
    }
    SetPhaseDurationMs(PhaseId::RansomActive, static_cast<std::uint32_t>(config_.infectionDurationSec) * 1000);

    trayIcon_ = std::make_unique<TrayIcon>("RANS0M");
    trayIcon_->SetHardmodeChecked(consent_->IsEnabled());
    trayIcon_->onClose = [this] { running_ = false; };
    trayIcon_->onToggleHardmode = [this] {
        if (consent_->IsEnabled()) {
            consent_->Disable();
        } else if (Platform::ConfirmHardmodeEnable()) {
            consent_->Enable();
        }
        trayIcon_->SetHardmodeChecked(consent_->IsEnabled());
    };
    trayIcon_->onOpenConfig = [this] {
        if (Platform::ShowConfigDialog(config_)) {
            config_.Save(ConfigDir());
            SetPhaseDurationMs(PhaseId::RansomActive, static_cast<std::uint32_t>(config_.infectionDurationSec) * 1000);
            idleTimerMs_ = RollIdleTimerMs(); // apply new spawn bounds immediately
        }
    };

    sequencer_.SetOnPhaseEnter([this](const PhaseSpec& phase) { OnPhaseEnter(phase); });

    idleTimerMs_ = RollIdleTimerMs();
    return true;
}

std::uint32_t App::RollIdleTimerMs() {
    int minSec = std::max(0, config_.minSpawnDelaySec);
    int maxSec = std::max(minSec, config_.maxSpawnDelaySec);
    std::uint32_t minMs = static_cast<std::uint32_t>(minSec) * 1000;
    std::uint32_t rangeMs = static_cast<std::uint32_t>(maxSec - minSec) * 1000;
    return minMs + (rangeMs > 0 ? rng_() % rangeMs : 0);
}

void App::Shutdown() {
    Platform::UninstallKeyboardHook();
    trayIcon_.reset();
    ransomWindow_.reset();
    thankYouWindow_.reset();
    tauntWindows_.clear();
    iconBlockOverlay_.reset();
    warningIcon_.reset();
    text_.reset();
    overlay_.reset();
    audio_.Shutdown();
    TTF_Quit();
    IMG_Quit();
    SDL_Quit();
}

void App::OnPhaseEnter(const PhaseSpec& phase) {
    if (!phase.music.empty()) audio_.PlayMusicLoop(phase.music);
    if (!phase.sfx.empty()) audio_.PlaySfx(phase.sfx);

    if (phase.id != PhaseId::Warning) warningIcon_.reset();

    switch (phase.id) {
        case PhaseId::Idle:
            ransomWindow_.reset();
            thankYouWindow_.reset();
            tauntWindows_.clear();
            iconBlockOverlay_.reset();
            ransomFlashWindow_.reset();
            audio_.StopMusic();
            break;

        case PhaseId::Warning: {
            constexpr int kFaceW = 192, kFaceH = 191;
            warningFaceX_ = static_cast<int>(rng_() % static_cast<unsigned>(screenW_ - kFaceW > 0 ? screenW_ - kFaceW : 1));
            warningFaceY_ = static_cast<int>(rng_() % static_cast<unsigned>(screenH_ - kFaceH > 0 ? screenH_ - kFaceH : 1));

            warningIcon_ = std::make_unique<Window>("A-90", 200, 200,
                                                      SDL_WINDOW_ALWAYS_ON_TOP | SDL_WINDOW_SKIP_TASKBAR);
            if (warningIcon_->Valid()) {
                warningIcon_->SetPosition(warningFaceX_, warningFaceY_);
                warningFaceTex_ = warningIcon_->LoadTexture(AssetPath("images/ransom_idle.png").string());
                warningStopTex_ = warningIcon_->LoadTexture(AssetPath("images/stop_sign.png").string());
                Platform::MakeWindowColorKeyTransparent(warningIcon_->Raw(), 0, 0, 192);
            }
            spySnapshotTaken_ = false;
            warningMoved_ = false;
            globalKeyPressed_ = false;
            break;
        }

        case PhaseId::DownloadJumpscare:
            installSigns_.clear();
            installSfxPlayed_ = false;
            break;

        case PhaseId::RansomActive: {
            coins_->ScatterRandomCoins(8);
            ransomWindow_ = std::make_unique<RansomWindow>(
                screenW_, screenH_, *coins_, AssetPath("images/ransom_idle.png").string(),
                AssetPath("images/Gold.png").string(), AssetPath("fonts/Cousine-Bold.ttf").string(),
                config_.ransomAmount);
            ransomWindow_->onFullyPaid = [this] { sequencer_.ReportSignal(); };
            ransomWindow_->onWantsMoreTaunt = [this] { SpawnTaunt(); };
            ransomWindow_->onCoinRedeemed = [this] { audio_.PlaySfx("cash"); };
            for (int i = 0; i < 6; ++i) SpawnTaunt();
            ransomFlashAlpha_ = 1.0f;
            ransomMusicStage_ = 0;

            SDL_Rect bounds{};
            SDL_GetDisplayBounds(0, &bounds);
            iconBlockOverlay_ = std::make_unique<IconBlockOverlay>(
                bounds.x, bounds.y, screenW_, screenH_, AssetPath("images/stop_sign.png").string());

            ransomFlashWindow_ = std::make_unique<Window>("A-90", 400, 400,
                SDL_WINDOW_ALWAYS_ON_TOP | SDL_WINDOW_SKIP_TASKBAR, /*startVisible=*/false);
            if (ransomFlashWindow_->Valid()) {
                texRansomRandom_ = ransomFlashWindow_->LoadTexture(AssetPath("images/ransom_random.png").string());
                Platform::MakeWindowColorKeyTransparent(ransomFlashWindow_->Raw(), 0, 0, 192);
            }
            flashBurstRemaining_ = 0;
            flashFaceElapsedMs_ = 0;
            flashNextBurstMs_ = rng_() % 5000; // matches Random.Next(5000)
            break;
        }

        case PhaseId::Resolved:
            ransomWindow_.reset();
            tauntWindows_.clear();
            iconBlockOverlay_.reset();
            ransomFlashWindow_.reset();
            audio_.StopMusic();
            thankYouWindow_ = std::make_unique<ThankYouWindow>(
                screenW_, screenH_, AssetPath("images/ok_sign.png").string(),
                AssetPath("fonts/Cousine-Bold.ttf").string());
            break;

        case PhaseId::TimedOut: {
            ransomWindow_.reset();
            tauntWindows_.clear();
            iconBlockOverlay_.reset();
            ransomFlashWindow_.reset();
            coins_->DeleteAllCoins();
            audio_.StopMusic();
            if (config_.execCmdOnDeath) Platform::RunOnDeathCommand(config_.cmdOnDeath);
            bool hardModeEnabled = consent_->IsEnabled();
            sequencer_.ReportHardModeDecision(hardModeEnabled);
            break;
        }

        case PhaseId::RealHardMode:
            // fires after shake beat
            hardShutdownFired_ = false;
            break;

        default:
            break;
    }
}

void App::SpawnTaunt() {
    const char* image = kTauntImages[rng_() % kTauntImages.size()];
    tauntWindows_.emplace_back(AssetPath(image).string(), screenW_, screenH_);
}

void App::HandleEvent(const SDL_Event& e) {
    switch (e.type) {
        case SDL_QUIT:
            running_ = false;
            break;

        case SDL_DROPFILE: {
            std::filesystem::path path(e.drop.file);
            if (ransomWindow_) ransomWindow_->HandleDropFile(path);
            SDL_free(e.drop.file);
            break;
        }

        default:
            break;
    }
}

void App::Update(std::uint32_t deltaMs) {
    // checked once, at phase end
    if (sequencer_.Current() == PhaseId::Warning) {
        std::uint32_t preT = sequencer_.ElapsedMs();

        if (preT >= 500 && preT < 1000 && !warningMoved_) {
            if (globalKeyPressed_) warningMoved_ = true; // keypress also counts

            int mx = 0, my = 0;
            SDL_GetGlobalMouseState(&mx, &my);
            if (!spySnapshotTaken_) {
                spyBasePos_ = SDL_Point{mx, my};
                spySnapshotTaken_ = true;
            } else if (mx != spyBasePos_.x || my != spyBasePos_.y) {
                warningMoved_ = true;
            }
        }

        const PhaseSpec& warningPhase = FindPhase(PhaseId::Warning);
        if (warningMoved_ && preT + deltaMs >= warningPhase.durationMs) {
            sequencer_.ReportSignal(); // fires at natural timeout, no gap
        }
    }

    sequencer_.Update(deltaMs);
    PhaseId current = sequencer_.Current();
    std::uint32_t t = sequencer_.ElapsedMs();

    trayIcon_->SetCloseEnabled(current == PhaseId::Idle);
    trayIcon_->Pump();

    if (current == PhaseId::Idle && config_.spawnAutomatically) {
        if (idleTimerMs_ <= deltaMs) {
            sequencer_.Start();
            idleTimerMs_ = RollIdleTimerMs();
        } else {
            idleTimerMs_ -= deltaMs;
        }
    }

    if (current == PhaseId::DownloadJumpscare && t >= 800 && !installSfxPlayed_) {
        audio_.PlaySfx("install");
        installSfxPlayed_ = true;
    }

    // layer1 at t=0, layer2 at 26s, layer3 at 52s
    if (current == PhaseId::RansomActive) {
        if (t >= 52000 && ransomMusicStage_ < 2) {
            audio_.PlayMusicLoop("layer3");
            ransomMusicStage_ = 2;
        } else if (t >= 26000 && ransomMusicStage_ < 1) {
            audio_.PlayMusicLoop("layer2");
            ransomMusicStage_ = 1;
        }
        UpdateRansomFlash(deltaMs);
    }

    topMostAccumMs_ += deltaMs;
    if (topMostAccumMs_ >= 500) {
        topMostAccumMs_ = 0;
        if (overlay_) SDL_SetWindowAlwaysOnTop(overlay_->Raw(), SDL_TRUE);
        if (warningIcon_) SDL_SetWindowAlwaysOnTop(warningIcon_->Raw(), SDL_TRUE);
        if (ransomFlashWindow_) SDL_SetWindowAlwaysOnTop(ransomFlashWindow_->Raw(), SDL_TRUE);
    }

    if (current == PhaseId::RealHardMode && t >= 1000 && !hardShutdownFired_) {
        hardShutdownFired_ = true;
        Platform::RealHardShutdown();
    }

    if (ransomWindow_) ransomWindow_->Update(deltaMs);

    for (auto& taunt : tauntWindows_) taunt.Update(deltaMs);
    tauntWindows_.erase(std::remove_if(tauntWindows_.begin(), tauntWindows_.end(),
                                        [](const TauntWindow& t) { return t.Expired(); }),
                         tauntWindows_.end());
}

void App::DrawCenteredShaking(SDL_Texture* tex, int size) {
    if (!tex) return;
    int offX = static_cast<int>(rng_() % 81) - 40;
    int offY = static_cast<int>(rng_() % 81) - 40;
    SDL_Rect dst{screenW_ / 2 - size / 2 + offX, screenH_ / 2 - size / 2 + offY, size, size};
    SDL_RenderCopy(overlay_->Renderer(), tex, nullptr, &dst);
}

// Ransomed()'s "Random flashing Ransom faces" thread: every ~0-5s, 2-5 faces (size
// 50-400, random pos) flash one at a time, ~25ms each.
void App::UpdateRansomFlash(std::uint32_t deltaMs) {
    if (!ransomFlashWindow_ || !ransomFlashWindow_->Valid()) return;

    auto placeNextFace = [this] {
        flashFaceSize_ = 50 + static_cast<int>(rng_() % 351); // Next(50, 400)
        int maxX = screenW_ - flashFaceSize_ > 0 ? screenW_ - flashFaceSize_ : 1;
        int maxY = screenH_ - flashFaceSize_ > 0 ? screenH_ - flashFaceSize_ : 1;
        ransomFlashWindow_->SetPosition(static_cast<int>(rng_() % maxX), static_cast<int>(rng_() % maxY));
    };

    if (flashBurstRemaining_ > 0) {
        flashFaceElapsedMs_ += deltaMs;
        if (flashFaceElapsedMs_ >= 25) {
            flashFaceElapsedMs_ = 0;
            if (--flashBurstRemaining_ > 0) {
                placeNextFace();
            } else {
                ransomFlashWindow_->Hide();
                flashNextBurstMs_ = rng_() % 5000; // matches Random.Next(5000)
            }
        }
    } else if (flashNextBurstMs_ <= deltaMs) {
        // for(i=0; i<=Next(1,5); i++) runs N+1 times, N in [1,4] -> 2..5 flashes
        flashBurstRemaining_ = 2 + static_cast<int>(rng_() % 4);
        flashFaceElapsedMs_ = 0;
        placeNextFace();
        ransomFlashWindow_->Show();
    } else {
        flashNextBurstMs_ -= deltaMs;
    }
}

void App::RenderRansomFlash() {
    if (!ransomFlashWindow_ || !ransomFlashWindow_->Valid() || flashBurstRemaining_ <= 0) return;

    ransomFlashWindow_->Clear(0, 0, 192); // see MakeWindowColorKeyTransparent
    if (texRansomRandom_) {
        SDL_Rect dst{0, 0, flashFaceSize_, flashFaceSize_};
        SDL_RenderCopy(ransomFlashWindow_->Renderer(), texRansomRandom_, nullptr, &dst);
    }
    ransomFlashWindow_->Present();
}

void App::RenderWarning(std::uint32_t t) {
    if (t < 1000) {
        SDL_SetWindowOpacity(overlay_->Raw(), 0.0f);
        if (warningIcon_ && warningIcon_->Valid()) {
            constexpr int kStopSize = 200;
            int w, h, x, y;
            if (t < 500) {
                w = 192; h = 191; x = warningFaceX_; y = warningFaceY_;
            } else {
                w = kStopSize; h = kStopSize;
                x = screenW_ / 2 - kStopSize / 2;
                y = screenH_ / 2 - kStopSize / 2;
            }

            Platform::ShowAndForceTopmost(warningIcon_->Raw(), x, y, 200, 200);

            warningIcon_->Clear(0, 0, 192); // see MakeWindowColorKeyTransparent
            SDL_Texture* tex = (t < 500) ? warningFaceTex_ : warningStopTex_;
            if (tex) {
                SDL_Rect dst{0, 0, w, h};
                SDL_RenderCopy(warningIcon_->Renderer(), tex, nullptr, &dst);
            }
            warningIcon_->Present();
        }
        return;
    }

    if (warningIcon_) warningIcon_->Hide();
    SDL_SetWindowOpacity(overlay_->Raw(), 1.0f);
    overlay_->Clear(139, 0, 0);
    if (texRansomIdle_) {
        SDL_Rect dst{screenW_ / 2 - 110, screenH_ / 2 - 110, 220, 220};
        SDL_RenderCopy(overlay_->Renderer(), texRansomIdle_, nullptr, &dst);
    }
    overlay_->Present();
}

void App::RenderDownloadJumpscare(std::uint32_t t) {
    SDL_SetWindowOpacity(overlay_->Raw(), 1.0f);
    overlay_->Clear(139, 0, 0); // constant, no flicker

    if (t < 800) {
        DrawCenteredShaking(texRansomAttack_, 900); // pc_attack: Size(900,900), Zoom
    } else {
        std::uint32_t sinceInstall = t - 800;
        std::size_t target = std::min<std::size_t>(71, sinceInstall / 10);
        while (installSigns_.size() < target) {
            int maxX = screenW_ - 192 > 0 ? screenW_ - 192 : 1;
            int maxY = screenH_ - 192 > 0 ? screenH_ - 192 : 1;
            installSigns_.push_back(SDL_Point{static_cast<int>(rng_() % static_cast<unsigned>(maxX)),
                                               static_cast<int>(rng_() % static_cast<unsigned>(maxY))});
        }
        if (texStopSign_) {
            for (const auto& p : installSigns_) {
                SDL_Rect dst{p.x, p.y, 192, 192};
                SDL_RenderCopy(overlay_->Renderer(), texStopSign_, nullptr, &dst);
            }
        }

        int jitterX = static_cast<int>(rng_() % 11) - 5;
        int jitterY = static_cast<int>(rng_() % 11) - 5;

        if (text_) {
            TextRenderer::Text label = text_->Get("DOWNLOADING...", 36, SDL_Color{240, 240, 240, 255});
            if (label.texture) {
                SDL_Rect dst{screenW_ / 2 - label.w / 2 + jitterX, screenH_ / 2 - 40 + jitterY, label.w, label.h};
                SDL_RenderCopy(overlay_->Renderer(), label.texture, nullptr, &dst);
            }
        }

        // measured off progress_bar.png/progress_elem.png directly
        constexpr float kNativeBarW = 2022.f, kNativeBarH = 135.f;
        constexpr float kContentX = 18.f, kContentY = 19.f;
        constexpr float kElemW = 194.f, kElemH = 96.f, kMargin = 0.f, kGap = 9.f;
        constexpr int kElemCount = 9; // max that fits, ~169px track left over
        constexpr float kTargetBarW = 480.f;

        float scale = kTargetBarW / kNativeBarW;
        int targetBarH = static_cast<int>(kNativeBarH * scale);
        int barX = screenW_ / 2 - static_cast<int>(kTargetBarW / 2) + jitterX;
        int barY = screenH_ / 2 + 20 + jitterY;

        if (texProgressBar_) {
            SDL_Rect barDst{barX, barY, static_cast<int>(kTargetBarW), targetBarH};
            SDL_RenderCopy(overlay_->Renderer(), texProgressBar_, nullptr, &barDst);
        }

        float fillFrac = std::min(1.0f, std::max(0.0f, (t - 800) / 1200.0f));
        int filledCount = static_cast<int>(fillFrac * kElemCount + 0.5f);

        if (texProgressElem_) {
            for (int i = 0; i < filledCount; ++i) {
                float nx = kContentX + kMargin + i * (kElemW + kGap);
                SDL_Rect elemDst{barX + static_cast<int>(nx * scale), barY + static_cast<int>(kContentY * scale),
                                  static_cast<int>(kElemW * scale), static_cast<int>(kElemH * scale)};
                SDL_RenderCopy(overlay_->Renderer(), texProgressElem_, nullptr, &elemDst);
            }
        }
    }

    overlay_->Present();
}

void App::RenderCrashBeat(std::uint32_t t, bool /*cutToBlackAfter*/) {
    SDL_SetWindowOpacity(overlay_->Raw(), 1.0f);
    if (t < 1000) {
        overlay_->Clear(139, 0, 0);
        DrawCenteredShaking(texRansomAttack_, 900); // pc_attack: Size(900,900), Zoom
    } else {
        overlay_->Clear(0, 0, 0);
    }
    overlay_->Present();
}

void App::Render() {
    PhaseId current = sequencer_.Current();
    std::uint32_t t = sequencer_.ElapsedMs();

    switch (current) {
        case PhaseId::Warning:
            RenderWarning(t);
            break;

        case PhaseId::DownloadJumpscare:
            RenderDownloadJumpscare(t);
            break;

        case PhaseId::RansomActive:
            if (ransomFlashAlpha_ > 0.f) {
                ransomFlashAlpha_ -= 0.12f; // ~50ms fade
                if (ransomFlashAlpha_ < 0.f) ransomFlashAlpha_ = 0.f;
                SDL_SetWindowOpacity(overlay_->Raw(), ransomFlashAlpha_);
                overlay_->Clear(255, 0, 0);
                overlay_->Present();
            } else {
                SDL_SetWindowOpacity(overlay_->Raw(), 0.0f);
            }
            RenderRansomFlash();
            break;

        case PhaseId::FakeCrash:
            RenderCrashBeat(t, /*cutToBlackAfter=*/true);
            break;

        case PhaseId::RealHardMode:
            RenderCrashBeat(t, /*cutToBlackAfter=*/false);
            break;

        default:
            SDL_SetWindowOpacity(overlay_->Raw(), 0.0f);
            break;
    }

    if (ransomWindow_) {
        const PhaseSpec& phase = FindPhase(PhaseId::RansomActive);
        std::uint32_t remaining = phase.durationMs > t ? phase.durationMs - t : 0;
        ransomWindow_->Render(remaining);
    }

    if (thankYouWindow_) thankYouWindow_->Render();

    if (iconBlockOverlay_) iconBlockOverlay_->Render();

    for (auto& taunt : tauntWindows_) taunt.Render();
}

int App::Run() {
    if (!Init()) {
        Shutdown();
        return 1;
    }

    std::uint32_t lastTicks = SDL_GetTicks();
    while (running_) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) HandleEvent(e);

        std::uint32_t now = SDL_GetTicks();
        std::uint32_t deltaMs = now - lastTicks;
        lastTicks = now;

        Update(deltaMs);
        Render();

        SDL_Delay(16);
    }

    Shutdown();
    return 0;
}

} // namespace rd
