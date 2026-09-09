#pragma once

#include <SDL.h>

#include <string>
#include <vector>

#include "Window.hpp"

namespace rd {

// like in DOORS blocks slots, but this blocks desktop icons
// Known gap: Win+D can still hide it (desktop rises above HWND_BOTTOM);
// WS_EX_TOPMOST would fix it but no, it will above real app windows
class IconBlockOverlay {
public:
    IconBlockOverlay(int screenX, int screenY, int screenW, int screenH, const std::string& stopSignPath);

    void Render();

private:
    Window window_;
    SDL_Texture* texture_ = nullptr;
    std::vector<SDL_Rect> localIconRects_; // already offset into this window's own coord space
};

} // namespace rd
