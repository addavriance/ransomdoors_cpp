#include "IconBlockOverlay.hpp"

#include "../platform/Platform.hpp"

namespace rd {

namespace {
constexpr Uint32 kRefreshIntervalMs = 2000;
}

IconBlockOverlay::IconBlockOverlay(int screenX, int screenY, int screenW, int screenH,
                                    const std::string& stopSignPath)
    // borderless, not topmost - see class comment
    : window_("", screenW, screenH, SDL_WINDOW_SKIP_TASKBAR, /*startVisible=*/false),
      screenX_(screenX), screenY_(screenY) {
    if (!window_.Valid()) return;
    window_.SetPosition(screenX, screenY);
    texture_ = window_.LoadTexture(stopSignPath);

    RefreshIconRects();
    lastRefreshMs_ = SDL_GetTicks();

    Platform::MakeWindowColorKeyTransparent(window_.Raw(), 0, 0, 192);
    Platform::MakeWindowNonActivating(window_.Raw()); // no focus steal on click
    window_.Show(); // colorkey set before showing
}

void IconBlockOverlay::RefreshIconRects() {
    localIconRects_.clear();
    for (const SDL_Rect& r : Platform::GetDesktopIconRects()) {
        localIconRects_.push_back(SDL_Rect{r.x - screenX_, r.y - screenY_, r.w, r.h});
    }
}

void IconBlockOverlay::Render() {
    Platform::RestoreIfMinimized(window_.Raw());

    Uint32 now = SDL_GetTicks();
    if (now - lastRefreshMs_ >= kRefreshIntervalMs) {
        RefreshIconRects(); // periodic, so a stale/empty snapshot (macOS) or new coin icons stay in sync
        lastRefreshMs_ = now;
    }

    window_.Clear(0, 0, 0, 0); // see MakeWindowColorKeyTransparent
    if (texture_) {
        for (const SDL_Rect& r : localIconRects_) SDL_RenderCopy(window_.Renderer(), texture_, nullptr, &r);
    }
    window_.Present();
    Platform::PinWindowToBottom(window_.Raw()); // click can nudge z-order
}

} // namespace rd
