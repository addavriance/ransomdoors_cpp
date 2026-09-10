#include "App.hpp"

#include <SDL_image.h>
#include <SDL_ttf.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>

#include "../platform/Platform.hpp"

namespace rd {

namespace {

constexpr std::array<const char*, 8> kTauntImages = {
    "images/glitch1.jpg", "images/glitch2.jpeg",   "images/glitch3.jpg",    "images/glitch4.jpg",
    "images/glitch5.jpg", "images/idiot.png",      "images/tauntface.png",  "images/tauntflower.png",
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
    // heals a crashed/force-killed previous run's infected cursor
    Platform::RestoreCursor();

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) return false;
    if (!(IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG) & (IMG_INIT_PNG | IMG_INIT_JPG))) return false;
    if (TTF_Init() != 0) return false;
    if (!audio_.Init()) return false;

    audio_.LoadSfx("spawn", AssetPath("sounds/spawn.wav").string());
    audio_.LoadSfx("install", AssetPath("sounds/install.wav").string());
    audio_.LoadSfx("cash", AssetPath("sounds/cash.wav").string());
    audio_.LoadSfx("attack", AssetPath("sounds/attack.wav").string());
    audio_.LoadSfx("thankyou", AssetPath("sounds/thankyou.wav").string());
    audio_.LoadSfx("crucifix", AssetPath("sounds/crucifix.wav").string());
    audio_.LoadSfx("tauntSpawn", AssetPath("sounds/tauntSpawn.wav").string());
    audio_.LoadSfx("tauntLeave", AssetPath("sounds/tauntLeave.wav").string());
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
    attackGif_ = std::make_unique<GifAnimation>(overlay_->Renderer(), AssetPath("images/ransom_attack.gif").string());
    staticGif_ = std::make_unique<GifAnimation>(overlay_->Renderer(), AssetPath("images/static.gif").string());

    text_ = std::make_unique<TextRenderer>(overlay_->Renderer());
    text_->LoadFont(AssetPath("fonts/Cousine-Bold.ttf").string());

    // preload gif (original size was 17mb lol)
    crucifixWindow_ = std::make_unique<CrucifixWindow>(screenW_, screenH_, AssetPath("images/repent.gif").string());
    vignetteWindow_ = std::make_unique<VignetteWindow>(bounds.x, bounds.y, screenW_, screenH_,
                                                          AssetPath("images/red_vignette.gif").string());

    coins_ = std::make_unique<CoinManager>(ConfigDir());
    Platform::RegisterFileTypeIcon(".gold1", AssetPath("images/Gold1.ico"));
    Platform::RegisterFileTypeIcon(".gold2", AssetPath("images/Gold2.ico"));
    Platform::RegisterFileTypeIcon(".gold3", AssetPath("images/Gold3.ico"));
    Platform::RegisterFileTypeIcon(".gold4", AssetPath("images/Gold4.ico"));
    Platform::RegisterFileTypeIcon(".gold5", AssetPath("images/Gold5.ico"));
    Platform::RegisterFileTypeIcon(".gold6", AssetPath("images/HoneyPot.ico"));
    Platform::RegisterFileTypeIcon(".crucifix", AssetPath("images/crucifix.ico"));
    Platform::InstallKeyboardHook([this] { globalKeyPressed_ = true; });

    configAssets_ = ConfigWindowAssets{AssetPath("fonts/Cousine-Bold.ttf").string(),
                                        AssetPath("images/Starlight.png").string()};

    bool firstRun = !std::filesystem::exists(ConfigDir() / "config.json");
    config_.Load(ConfigDir());
    if (firstRun) {
        if (ConfigWindow::ShowModal(config_, configAssets_)) config_.Save(ConfigDir());
    }
    SetPhaseDurationMs(PhaseId::RansomActive, static_cast<std::uint32_t>(config_.infectionDurationSec) * 1000);

    configWindow_.onAccepted = [this](const Config& cfg, bool spawnRequested) {
        config_ = cfg;
        config_.Save(ConfigDir());
        SetPhaseDurationMs(PhaseId::RansomActive, static_cast<std::uint32_t>(config_.infectionDurationSec) * 1000);
        idleTimerMs_ = RollIdleTimerMs(); // apply new spawn bounds immediately
        if (spawnRequested && sequencer_.Current() == PhaseId::Idle) sequencer_.Start();
    };

    trayIcon_ = std::make_unique<TrayIcon>("RANS0M");
    trayIcon_->onClose = [this] { running_ = false; };
    trayIcon_->onOpenConfig = [this] { configWindow_.Open(config_, configAssets_); };

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
    if (coins_) coins_->DeleteAllCoins(); // don't leave scattered coins behind on exit
    Platform::RestoreCursor();
    trayIcon_.reset();
    configWindow_.Close();
    ransomWindow_.reset();
    thankYouWindow_.reset();
    crucifixWindow_.reset();
    vignetteWindow_.reset();
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
            if (crucifixWindow_) crucifixWindow_->Hide();
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
            wasCrucifix_ = false;
            Platform::SetInfectedCursor(AssetPath("images/infectedcursor.cur"));
            if (vignetteWindow_) vignetteWindow_->Show();
            // no dark-red wallpaper for now: dead on Win11 24H2's DWM-composited desktop
            int targetGold = static_cast<int>(config_.ransomAmount * 1.2);
            if (config_.useDrawerMode) {
                coins_->ScatterDrawerCoins(targetGold, config_.infectionDurationSec);
                Platform::OpenFolder(coins_->DrawerFolderPath());
            } else {
                int maxDepth = std::clamp(2 + config_.infectionDurationSec / 45, 2, 8);
                coins_->ScatterRandomCoins(targetGold, maxDepth);
            }
            ransomWindow_ = std::make_unique<RansomWindow>(
                screenW_, screenH_, *coins_, AssetPath("images/ransom_idle.png").string(),
                AssetPath("images/Gold.png").string(), AssetPath("fonts/Cousine-Bold.ttf").string(),
                config_.ransomAmount);
            ransomWindow_->onFullyPaid = [this] { sequencer_.ReportSignal(); };
            ransomWindow_->onWantsMoreTaunt = [this] { SpawnTaunt(); };
            ransomWindow_->onCoinRedeemed = [this] { audio_.PlaySfx("cash"); };
            ransomWindow_->onCrucifix = [this] { wasCrucifix_ = true; audio_.PlaySfx("crucifix"); };
            for (int i = 0; i < 9; ++i) SpawnTaunt();
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
            flashNextBurstMs_ = rng_() % 5000;
            break;
        }

        case PhaseId::Resolved: {
            // captured before reset() below - ThankYou/CrucifixWindow spawn here
            SDL_Point ransomPos = ransomWindow_ ? ransomWindow_->Position() : SDL_Point{screenW_ / 2, screenH_ / 2};

            ransomWindow_.reset();
            tauntWindows_.clear();
            iconBlockOverlay_.reset();
            ransomFlashWindow_.reset();
            if (vignetteWindow_) vignetteWindow_->Hide();
            coins_->DeleteAllCoins(); // sweeps unredeemed leftovers too
            Platform::RestoreCursor();
            audio_.StopMusic();
            if (wasCrucifix_ && crucifixWindow_) {
                crucifixWindow_->SetPosition(ransomPos);
                crucifixWindow_->ResetAnimation();
                crucifixWindow_->Show();
                // phase ends right as the gif does
                SetPhaseDurationMs(PhaseId::Resolved, crucifixWindow_->GifDurationMs());
            } else {
                if (crucifixWindow_) crucifixWindow_->Hide();
                SetPhaseDurationMs(PhaseId::Resolved, 4700); // covers the full reveal animation
                thankYouWindow_ = std::make_unique<ThankYouWindow>(
                    screenW_, screenH_, ransomPos, AssetPath("images/ransom_idle.png").string(),
                    AssetPath("images/ok_sign.png").string(), AssetPath("images/thx_txt.png").string());
                thankYouWindow_->onReveal = [this] { audio_.PlaySfx("thankyou"); };
            }
            break;
        }

        case PhaseId::TimedOut: {
            ransomWindow_.reset();
            tauntWindows_.clear();
            iconBlockOverlay_.reset();
            ransomFlashWindow_.reset();

            if (vignetteWindow_) vignetteWindow_->Hide();
            coins_->DeleteAllCoins();
            audio_.StopMusic();

            if (config_.execCmdOnDeath) Platform::RunOnDeathCommand(config_.cmdOnDeath);
            sequencer_.ReportHardModeDecision(config_.crashOnDeath);
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
    audio_.PlaySfx("tauntSpawn");
}

void App::HandleEvent(const SDL_Event& e) {
    configWindow_.HandleEvent(e);

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


    if (current == PhaseId::RansomActive) {
        int layer3StartSec = std::max(0, config_.infectionDurationSec - 26);
        std::uint32_t layer3Ms = static_cast<std::uint32_t>(layer3StartSec) * 1000;
        std::uint32_t layer2Ms = layer3Ms / 2;
        if (t >= layer3Ms && ransomMusicStage_ < 2) {
            audio_.PlayMusicLoop("layer3");
            ransomMusicStage_ = 2;
        } else if (t >= layer2Ms && ransomMusicStage_ < 1) {
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
    if (crucifixWindow_) crucifixWindow_->Update(deltaMs);
    if (thankYouWindow_) thankYouWindow_->Update(deltaMs);
    if (attackGif_) attackGif_->Update(deltaMs);
    if (staticGif_) staticGif_->Update(deltaMs);
    if (vignetteWindow_) vignetteWindow_->Update(deltaMs);

    for (auto& taunt : tauntWindows_) taunt.Update(deltaMs);
    tauntWindows_.erase(std::remove_if(tauntWindows_.begin(), tauntWindows_.end(),
                                        [this](const TauntWindow& t) {
                                            if (!t.Expired()) return false;
                                            audio_.PlaySfx("tauntLeave");
                                            return true;
                                        }),
                         tauntWindows_.end());
}

void App::DrawCenteredShaking(SDL_Texture* tex, int size) {
    if (!tex) return;
    int offX = static_cast<int>(rng_() % 81) - 40;
    int offY = static_cast<int>(rng_() % 81) - 40;
    SDL_Rect dst{screenW_ / 2 - size / 2 + offX, screenH_ / 2 - size / 2 + offY, size, size};
    SDL_RenderCopy(overlay_->Renderer(), tex, nullptr, &dst);
}

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
                flashNextBurstMs_ = rng_() % 5000;
            }
        }
    } else if (flashNextBurstMs_ <= deltaMs) {
        flashBurstRemaining_ = 2 + static_cast<int>(rng_() % 4); // 2..5 flashes
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

    if (staticGif_ && staticGif_->Valid()) {
        SDL_Texture* frame = staticGif_->CurrentFrame();
        SDL_SetTextureColorMod(frame, 255, 60, 60);
        SDL_SetTextureAlphaMod(frame, 40);
        SDL_Rect dst{0, 0, screenW_, screenH_};
        SDL_RenderCopy(overlay_->Renderer(), frame, nullptr, &dst);
        SDL_SetTextureColorMod(frame, 255, 255, 255);
        SDL_SetTextureAlphaMod(frame, 255);
    }

    if (t < 800) {
        DrawCenteredShaking(attackGif_ && attackGif_->Valid() ? attackGif_->CurrentFrame() : texRansomAttack_, 900);
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
            static const char* kDownloadStates[5] = {"DOWNLOADING", "DOWNLOADING.", "DOWNLOADING..",
                                                       "DOWNLOADING...", "DOWNLOADING"};
            int step = static_cast<int>((sinceInstall / 120) % 5);
            SDL_Color color = step == 4 ? SDL_Color{255, 0, 0, 255} : SDL_Color{255, 255, 255, 255};
            TextRenderer::Text label = text_->Get(kDownloadStates[step], 36, color);
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
        if (staticGif_ && staticGif_->Valid()) {
            SDL_Texture* frame = staticGif_->CurrentFrame();
            SDL_SetTextureColorMod(frame, 255, 60, 60);
            SDL_SetTextureAlphaMod(frame, 40);
            SDL_Rect dst{0, 0, screenW_, screenH_};
            SDL_RenderCopy(overlay_->Renderer(), frame, nullptr, &dst);
            SDL_SetTextureColorMod(frame, 255, 255, 255);
            SDL_SetTextureAlphaMod(frame, 255);
        }
        DrawCenteredShaking(attackGif_ && attackGif_->Valid() ? attackGif_->CurrentFrame() : texRansomAttack_, 900);
    } else {
        overlay_->Clear(0, 0, 0);
    }
    overlay_->Present();
}

void App::Render() {
    configWindow_.Render();

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
    if (crucifixWindow_) crucifixWindow_->Render();

    if (iconBlockOverlay_) iconBlockOverlay_->Render();
    if (vignetteWindow_) vignetteWindow_->Render();

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
