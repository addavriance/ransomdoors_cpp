#pragma once

#include <memory>
#include <string>

#include "GifAnimation.hpp"
#include "Window.hpp"

namespace rd {

class VignetteWindow {
public:
    VignetteWindow(int screenX, int screenY, int screenW, int screenH, const std::string& gifPath);

    void Show();
    void Hide();

    void Update(std::uint32_t deltaMs);
    void Render();

private:
    Window window_;
    std::unique_ptr<GifAnimation> gif_;
};

} // namespace rd
