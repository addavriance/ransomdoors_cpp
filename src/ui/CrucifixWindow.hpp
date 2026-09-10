#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "GifAnimation.hpp"
#include "Window.hpp"

namespace rd {

class CrucifixWindow {
public:
    CrucifixWindow(int screenW, int screenH, const std::string& repentGifPath);

    std::uint32_t GifDurationMs() const;

    void SetPosition(SDL_Point pos);

    void Show();
    void Hide();
    void ResetAnimation();

    void Update(std::uint32_t deltaMs);
    void Render();

private:
    Window window_;
    std::unique_ptr<GifAnimation> repent_;
};

} // namespace rd
