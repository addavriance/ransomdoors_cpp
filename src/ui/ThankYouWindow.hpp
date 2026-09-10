#pragma once

#include <cstdint>
#include <functional>
#include <string>

#include "Window.hpp"

namespace rd {

class ThankYouWindow {
public:
    ThankYouWindow(int screenW, int screenH, SDL_Point initialPos, const std::string& ransomFacePath,
                   const std::string& okSignPath, const std::string& thxTextPath);

    void Update(std::uint32_t deltaMs);
    void Render();

    // Fires once, right as the ok_sign reveal begins.
    std::function<void()> onReveal;

private:
    Window window_;
    SDL_Texture* texRansomFace_ = nullptr;
    SDL_Texture* texOkSign_ = nullptr;
    SDL_Texture* texThx_ = nullptr;
    SDL_Point initialPos_;
    SDL_Point targetPos_;
    std::uint32_t elapsedMs_ = 0;
    bool revealFired_ = false;
};

} // namespace rd
