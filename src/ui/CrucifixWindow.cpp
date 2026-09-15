#include "CrucifixWindow.hpp"

#include "../platform/Platform.hpp"

namespace rd {

namespace {
constexpr int kWidth = 800;
constexpr int kHeight = 800;
}

CrucifixWindow::CrucifixWindow(int screenW, int screenH, const std::string& repentGifPath)
    : window_("RANS0M", kWidth, kHeight, SDL_WINDOW_ALWAYS_ON_TOP | SDL_WINDOW_SKIP_TASKBAR,
              /*startVisible=*/false, /*titleBar=*/false) {
    if (!window_.Valid()) return;
    window_.SetPosition((screenW - kWidth) / 2, (screenH - kHeight) / 2);
    Platform::MakeWindowColorKeyTransparent(window_.Raw(), 0, 0, 0);
    repent_ = std::make_unique<GifAnimation>(window_.Renderer(), repentGifPath);
    repent_->SetLoop(false);
}

std::uint32_t CrucifixWindow::GifDurationMs() const {
    return repent_ && repent_->Valid() ? repent_->TotalDurationMs() : 3000;
}

void CrucifixWindow::SetPosition(SDL_Point pos) { window_.SetPosition(pos.x, pos.y); }

void CrucifixWindow::Show() { window_.Show(); }
void CrucifixWindow::Hide() { window_.Hide(); }
void CrucifixWindow::ResetAnimation() {
    if (repent_) repent_->Reset();
}

void CrucifixWindow::Update(std::uint32_t deltaMs) {
    if (repent_) repent_->Update(deltaMs);
}

void CrucifixWindow::Render() {
    SDL_Renderer* renderer = window_.Renderer();
    window_.Clear(0, 0, 0, 0); // see MakeWindowColorKeyTransparent

    if (repent_ && repent_->CurrentFrame()) {
        SDL_Rect dst{0, 0, kWidth, kHeight};
        SDL_RenderCopy(renderer, repent_->CurrentFrame(), nullptr, &dst);
    }

    window_.Present();
}

} // namespace rd
