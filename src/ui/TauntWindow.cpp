#include "TauntWindow.hpp"

#include <cstdlib>

namespace rd {

namespace {
std::string RandomTitle() {
    int len = 6 + std::rand() % 3; // 6-8 chars
    std::string s(len, ' ');
    for (int i = 0; i < len; ++i) s[i] = static_cast<char>('A' + std::rand() % 26);
    return s;
}
} // namespace

TauntWindow::TauntWindow(const std::string& imagePath, int screenW, int screenH)
    : width_(200 + std::rand() % 201),
      height_(200 + std::rand() % 201),
      window_(RandomTitle(), width_, height_, SDL_WINDOW_ALWAYS_ON_TOP | SDL_WINDOW_SKIP_TASKBAR,
              /*startVisible=*/true, /*titleBar=*/true),
      lifetimeMs_(4000 + std::rand() % 6001) {
    if (!window_.Valid()) return;

    texture_ = window_.LoadTexture(imagePath);

    int x = std::rand() % (screenW - width_ > 0 ? screenW - width_ : 1);
    int y = std::rand() % (screenH - height_ > 0 ? screenH - height_ : 1);
    ticker_.SetBase(x, y);
    window_.SetPosition(x, y);
}

void TauntWindow::Update(std::uint32_t deltaMs) {
    lifetimeMs_ -= static_cast<int>(deltaMs);
    if (ticker_.Tick(deltaMs)) {
        window_.SetPosition(ticker_.X(), ticker_.Y());
        SDL_SetWindowAlwaysOnTop(window_.Raw(), SDL_TRUE);
    }
}

void TauntWindow::Render() {
    window_.Clear(139, 0, 0);
    if (texture_) {
        SDL_Rect dst{0, 0, width_, height_};
        SDL_RenderCopy(window_.Renderer(), texture_, nullptr, &dst);
    }
    window_.Present();
}

} // namespace rd
