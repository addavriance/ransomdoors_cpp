#pragma once

#include <SDL.h>

#include <filesystem>
#include <functional>
#include <string>

#include "GlitchTicker.hpp"
#include "Text.hpp"
#include "Window.hpp"
#include "../coins/CoinManager.hpp"

namespace rd {

// 544x315. Uses SDL_DROPFILE for coins.
class RansomWindow {
public:
    RansomWindow(int screenW, int screenH, CoinManager& coins, const std::string& ransomIdlePath,
                 const std::string& goldPath, const std::string& fontPath, int ransomAmount = 500);

    Uint32 WindowId() const { return window_.Id(); }
    SDL_Point Position() const { return window_.Position(); }

    void HandleDropFile(const std::filesystem::path& path);
    void Update(std::uint32_t deltaMs);
    void Render(std::uint32_t remainingMs);

    std::function<void()> onFullyPaid;
    std::function<void()> onWantsMoreTaunt; // 2%/200ms relocate+spawn
    std::function<void()> onCoinRedeemed;
    std::function<void()> onCrucifix; // fired before onFullyPaid on a .crucifix pickup

private:
    Window window_;
    CoinManager& coins_;
    GlitchTicker ticker_;
    TextRenderer text_;
    SDL_Texture* texRansomIdle_ = nullptr;
    SDL_Texture* texGold_ = nullptr;
    int screenW_;
    int screenH_;
    int ransomLeft_; // -per-coin value, paid at <=0; initialized from ctor's ransomAmount
};

} // namespace rd
