#include "VignetteWindow.hpp"

#include "../platform/Platform.hpp"

namespace rd {

VignetteWindow::VignetteWindow(int screenX, int screenY, int screenW, int screenH, const std::string& gifPath)
    : window_("RANS0M", screenW, screenH, SDL_WINDOW_ALWAYS_ON_TOP | SDL_WINDOW_SKIP_TASKBAR,
              /*startVisible=*/false, /*titleBar=*/false) {
    if (!window_.Valid()) return;
    window_.SetPosition(screenX, screenY);
    Platform::MakeWindowColorKeyTransparent(window_.Raw(), 0, 0, 0);
    Platform::MakeWindowClickThrough(window_.Raw());
    gif_ = std::make_unique<GifAnimation>(window_.Renderer(), gifPath);
}

void VignetteWindow::Show() { window_.Show(); }
void VignetteWindow::Hide() { window_.Hide(); }

void VignetteWindow::Update(std::uint32_t deltaMs) {
    if (gif_) gif_->Update(deltaMs);
}

void VignetteWindow::Render() {
    SDL_Renderer* renderer = window_.Renderer();
    window_.Clear(0, 0, 0, 0); // see MakeWindowColorKeyTransparent

    if (gif_ && gif_->CurrentFrame()) {
        int w = 0, h = 0;
        SDL_GetWindowSize(window_.Raw(), &w, &h);
        SDL_Rect dst{0, 0, w, h};
        SDL_RenderCopy(renderer, gif_->CurrentFrame(), nullptr, &dst);
    }

    window_.Present();
}

} // namespace rd
