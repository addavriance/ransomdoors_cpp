#pragma once

#include <SDL.h>

#include <string>
#include <vector>

#include "Window.hpp"

namespace rd {

// blocks desktop icons; known gap: Win+D can still hide it (desktop rises above HWND_BOTTOM)
class IconBlockOverlay {
public:
    IconBlockOverlay(int screenX, int screenY, int screenW, int screenH, const std::string& stopSignPath);

    void Render();

private:
    void RefreshIconRects();

    Window window_;
    SDL_Texture* texture_ = nullptr;
    std::vector<SDL_Rect> localIconRects_; // already offset into this window's own coord space
    int screenX_ = 0;
    int screenY_ = 0;
    Uint32 lastRefreshMs_ = 0;
};

} // namespace rd
