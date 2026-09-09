#include "IconBlockOverlay.hpp"

#include "../platform/Platform.hpp"

namespace rd {

IconBlockOverlay::IconBlockOverlay(int screenX, int screenY, int screenW, int screenH,
                                    const std::string& stopSignPath)
    // borderless, not topmost - see class comment
    : window_("", screenW, screenH, SDL_WINDOW_SKIP_TASKBAR, /*startVisible=*/false) {
    if (!window_.Valid()) return;
    window_.SetPosition(screenX, screenY);
    texture_ = window_.LoadTexture(stopSignPath);

    for (const SDL_Rect& r : Platform::GetDesktopIconRects()) {
        localIconRects_.push_back(SDL_Rect{r.x - screenX, r.y - screenY, r.w, r.h});
    }

    Platform::MakeWindowColorKeyTransparent(window_.Raw(), 0, 0, 192);
    Platform::MakeWindowNonActivating(window_.Raw()); // no focus steal on click
    window_.Show(); // colorkey set before showing
}

void IconBlockOverlay::Render() {
    Platform::RestoreIfMinimized(window_.Raw());

    window_.Clear(0, 0, 192); // see MakeWindowColorKeyTransparent
    if (texture_) {
        for (const SDL_Rect& r : localIconRects_) SDL_RenderCopy(window_.Renderer(), texture_, nullptr, &r);
    }
    window_.Present();
    Platform::PinWindowToBottom(window_.Raw()); // click can nudge z-order
}

} // namespace rd
