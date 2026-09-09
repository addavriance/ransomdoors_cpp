#pragma once

#include <string>

#include "Text.hpp"
#include "Window.hpp"

namespace rd {

// visual only, App owns the Resolved-phase logic
class ThankYouWindow {
public:
    ThankYouWindow(int screenW, int screenH, const std::string& okSignPath, const std::string& fontPath);

    void Render();

private:
    Window window_;
    TextRenderer text_;
    SDL_Texture* texOkSign_ = nullptr;
};

} // namespace rd
