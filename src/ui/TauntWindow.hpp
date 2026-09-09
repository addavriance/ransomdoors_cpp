#pragma once

#include <SDL.h>

#include <string>

#include "GlitchTicker.hpp"
#include "Window.hpp"

namespace rd {

class TauntWindow {
public:
    TauntWindow(const std::string& imagePath, int screenW, int screenH);

    Uint32 WindowId() const { return window_.Id(); }
    bool Expired() const { return lifetimeMs_ <= 0; }

    void Update(std::uint32_t deltaMs);
    void Render();

private:
    int width_; // declared before window_ to size it
    int height_;
    Window window_;
    SDL_Texture* texture_ = nullptr;
    GlitchTicker ticker_;
    int lifetimeMs_;
};

} // namespace rd
