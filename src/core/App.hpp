#pragma once

#include <SDL.h>

#include <filesystem>
#include <memory>
#include <random>
#include <vector>

#include "Sequencer.hpp"
#include "../audio/AudioManager.hpp"
#include "../coins/CoinManager.hpp"
#include "../config/Config.hpp"
#include "../consent/HardModeConsent.hpp"
#include "../platform/TrayIcon.hpp"
#include "../ui/IconBlockOverlay.hpp"
#include "../ui/RansomWindow.hpp"
#include "../ui/TauntWindow.hpp"
#include "../ui/Text.hpp"
#include "../ui/ThankYouWindow.hpp"
#include "../ui/Window.hpp"

namespace rd {

class App {
public:
    int Run();

private:
    bool Init();
    void Shutdown();

    void OnPhaseEnter(const PhaseSpec& phase);
    void HandleEvent(const SDL_Event& e);
    void Update(std::uint32_t deltaMs);
    void Render();

    void RenderWarning(std::uint32_t t);
    void RenderDownloadJumpscare(std::uint32_t t);
    void RenderCrashBeat(std::uint32_t t, bool cutToBlackAfter);
    void DrawCenteredShaking(SDL_Texture* tex, int size);

    void UpdateRansomFlash(std::uint32_t deltaMs);
    void RenderRansomFlash();

    void SpawnTaunt();
    std::uint32_t RollIdleTimerMs();
    std::filesystem::path AssetPath(const std::string& relative) const;
    std::filesystem::path ConfigDir() const;

    Sequencer sequencer_;
    AudioManager audio_;
    Config config_;
    std::unique_ptr<CoinManager> coins_;
    std::unique_ptr<HardModeConsent> consent_;
    std::unique_ptr<TrayIcon> trayIcon_;
    std::unique_ptr<Window> overlay_;
    std::unique_ptr<RansomWindow> ransomWindow_;
    std::unique_ptr<ThankYouWindow> thankYouWindow_;
    std::vector<TauntWindow> tauntWindows_;
    std::unique_ptr<IconBlockOverlay> iconBlockOverlay_;

    // small opaque icon, not real transparency
    std::unique_ptr<Window> warningIcon_;
    SDL_Texture* warningFaceTex_ = nullptr;
    SDL_Texture* warningStopTex_ = nullptr;
    int warningFaceX_ = 0;
    int warningFaceY_ = 0;

    // bound to overlay_'s renderer
    SDL_Texture* texRansomIdle_ = nullptr;
    SDL_Texture* texStopSign_ = nullptr;
    SDL_Texture* texRansomAttack_ = nullptr;
    SDL_Texture* texProgressBar_ = nullptr;
    SDL_Texture* texProgressElem_ = nullptr;

    // Ransomed()'s random flashing faces (RansomActive only): colorkey transparent,
    // fixed max-size window, only a size x size sub-rect drawn per flash.
    std::unique_ptr<Window> ransomFlashWindow_;
    SDL_Texture* texRansomRandom_ = nullptr;
    std::uint32_t flashNextBurstMs_ = 0;
    int flashBurstRemaining_ = 0;
    std::uint32_t flashFaceElapsedMs_ = 0;
    int flashFaceSize_ = 0;

    // global cursor poll, not window events
    SDL_Point spyBasePos_{0, 0};
    bool spySnapshotTaken_ = false;
    bool warningMoved_ = false;
    bool globalKeyPressed_ = false;

    std::unique_ptr<TextRenderer> text_; // bound to overlay_'s renderer

    std::vector<SDL_Point> installSigns_;
    bool installSfxPlayed_ = false;
    bool hardShutdownFired_ = false;

    // sequential, not layered (SDL_mixer limit)
    int ransomMusicStage_ = 0;
    std::uint32_t topMostAccumMs_ = 0;

    float ransomFlashAlpha_ = 0.f;

    std::mt19937 rng_{std::random_device{}()};
    std::uint32_t idleTimerMs_ = 0;
    int screenW_ = 1280;
    int screenH_ = 720;
    bool running_ = true;
};

} // namespace rd
