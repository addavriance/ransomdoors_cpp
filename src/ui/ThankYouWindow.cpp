#include "ThankYouWindow.hpp"

namespace rd {

namespace {
constexpr int kWidth = 544;
constexpr int kHeight = 315;
}

ThankYouWindow::ThankYouWindow(int screenW, int screenH, const std::string& okSignPath,
                                const std::string& fontPath)
    : window_("RANS0M", kWidth, kHeight, SDL_WINDOW_ALWAYS_ON_TOP | SDL_WINDOW_SKIP_TASKBAR,
              /*startVisible=*/true, /*titleBar=*/true),
      text_(window_.Renderer()) {
    if (!window_.Valid()) return;
    window_.SetPosition((screenW - kWidth) / 2, (screenH - kHeight) / 2);
    text_.LoadFont(fontPath);
    texOkSign_ = window_.LoadTexture(okSignPath);
}

void ThankYouWindow::Render() {
    SDL_Renderer* renderer = window_.Renderer();
    window_.Clear(0, 192, 0);

    if (texOkSign_) {
        SDL_Rect dst{169, 83, 210, 210};
        SDL_RenderCopy(renderer, texOkSign_, nullptr, &dst);
    }

    TextRenderer::Text label = text_.Get("THANK YOU!", 40, SDL_Color{255, 255, 255, 255});
    if (label.texture) {
        SDL_Rect dst{79, 5, label.w, label.h};
        SDL_RenderCopy(renderer, label.texture, nullptr, &dst);
    }

    window_.Present();
}

} // namespace rd
