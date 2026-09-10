#include "ThankYouWindow.hpp"

#include <algorithm>
#include <array>

namespace rd {

namespace {
constexpr int kBaseWidth = 560;
constexpr int kBaseHeight = 350;
constexpr float kGrowScale = 1.3f;

constexpr std::uint32_t kGrowDurationMs = 500;
constexpr std::uint32_t kRevealAtMs = 500;
constexpr std::uint32_t kOkSignScaleMs = 300;
constexpr std::uint32_t kThxDelayMs = 400;
constexpr std::uint32_t kThxScaleMs = 200;

float EaseOutCubic(float t) {
    float inv = 1.0f - t;
    return 1.0f - inv * inv * inv;
}

struct Band {
    float startFrac;
    SDL_Color color;
};

constexpr std::array<Band, 5> kBands = {{
    {0.000f, {0x7D, 0xDC, 0x00, 255}},
    {0.255f, {0x5B, 0xBF, 0x00, 255}},
    {0.458f, {0x4A, 0xB1, 0x00, 255}},
    {0.632f, {0x30, 0x99, 0x00, 255}},
    {0.876f, {0x11, 0x7F, 0x00, 255}},
}};

void DrawStripedGreenBackground(SDL_Renderer* renderer, int w, int h) {
    for (size_t i = 0; i < kBands.size(); ++i) {
        float endFrac = (i + 1 < kBands.size()) ? kBands[i + 1].startFrac : 1.0f;
        SDL_Rect band{0, static_cast<int>(kBands[i].startFrac * h), w,
                       static_cast<int>((endFrac - kBands[i].startFrac) * h) + 1};
        const SDL_Color& c = kBands[i].color;
        SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, c.a);
        SDL_RenderFillRect(renderer, &band);
    }
}

} // namespace

ThankYouWindow::ThankYouWindow(int screenW, int screenH, SDL_Point initialPos, const std::string& ransomFacePath,
                                const std::string& okSignPath, const std::string& thxTextPath)
    : window_("RANS0M", kBaseWidth, kBaseHeight, SDL_WINDOW_ALWAYS_ON_TOP | SDL_WINDOW_SKIP_TASKBAR,
              /*startVisible=*/true, /*titleBar=*/true),
      initialPos_(initialPos),
      targetPos_{(screenW - static_cast<int>(kBaseWidth * kGrowScale)) / 2,
                 (screenH - static_cast<int>(kBaseHeight * kGrowScale)) / 2} {
    if (!window_.Valid()) return;
    window_.SetPosition(initialPos_.x, initialPos_.y);
    texRansomFace_ = window_.LoadTexture(ransomFacePath);
    texOkSign_ = window_.LoadTexture(okSignPath);
    texThx_ = window_.LoadTexture(thxTextPath);
}

void ThankYouWindow::Update(std::uint32_t deltaMs) {
    std::uint32_t prevElapsed = elapsedMs_;
    elapsedMs_ += deltaMs;

    if (elapsedMs_ <= kGrowDurationMs) {
        float t = EaseOutCubic(elapsedMs_ / static_cast<float>(kGrowDurationMs));
        int w = static_cast<int>(kBaseWidth + (kBaseWidth * kGrowScale - kBaseWidth) * t);
        int h = static_cast<int>(kBaseHeight + (kBaseHeight * kGrowScale - kBaseHeight) * t);
        int x = static_cast<int>(initialPos_.x + (targetPos_.x - initialPos_.x) * t);
        int y = static_cast<int>(initialPos_.y + (targetPos_.y - initialPos_.y) * t);
        window_.SetSize(w, h);
        window_.SetPosition(x, y);
    } else if (prevElapsed <= kGrowDurationMs) {
        // snap to target on the last frame crossing the threshold
        window_.SetSize(static_cast<int>(kBaseWidth * kGrowScale), static_cast<int>(kBaseHeight * kGrowScale));
        window_.SetPosition(targetPos_.x, targetPos_.y);
    }

    if (!revealFired_ && elapsedMs_ >= kRevealAtMs) {
        revealFired_ = true;
        if (onReveal) onReveal();
    }
}

void ThankYouWindow::Render() {
    SDL_Renderer* renderer = window_.Renderer();
    int curW = 0, curH = 0;
    SDL_GetWindowSize(window_.Raw(), &curW, &curH);
    float scale = curW / static_cast<float>(kBaseWidth);

    if (elapsedMs_ < kRevealAtMs) {
        window_.Clear(0, 0, 0);
        if (texRansomFace_) {
            SDL_Rect dst{static_cast<int>(28 * scale), static_cast<int>(12 * scale), static_cast<int>(188 * scale),
                         static_cast<int>(188 * scale)};
            SDL_RenderCopy(renderer, texRansomFace_, nullptr, &dst);
        }
        window_.Present();
        return;
    }

    DrawStripedGreenBackground(renderer, curW, curH);

    std::uint32_t sinceReveal = elapsedMs_ - kRevealAtMs;

    if (texOkSign_) {
        float t = std::clamp(sinceReveal / static_cast<float>(kOkSignScaleMs), 0.0f, 1.0f);
        float revealScale = 0.1f + 0.9f * t;
        int baseW = static_cast<int>(194 * scale * revealScale);
        int baseH = static_cast<int>(194 * scale * revealScale);
        int anchorY = static_cast<int>((97 + 194 * 0.3f) * scale);
        SDL_Rect dst{(curW - baseW) / 2, anchorY - static_cast<int>(baseH * 0.3f), baseW, baseH};
        SDL_RenderCopy(renderer, texOkSign_, nullptr, &dst);
    }

    if (texThx_ && sinceReveal >= kThxDelayMs) {
        float t = std::clamp((sinceReveal - kThxDelayMs) / static_cast<float>(kThxScaleMs), 0.0f, 1.0f);
        float revealScale = 0.1f + 0.9f * t;
        int w = static_cast<int>(488 * scale * revealScale);
        int h = static_cast<int>(87 * scale * revealScale);
        int bottomY = static_cast<int>((14 + 87) * scale);
        SDL_Rect dst{(curW - w) / 2, bottomY - h, w, h};
        SDL_RenderCopy(renderer, texThx_, nullptr, &dst);
    }

    window_.Present();
}

} // namespace rd
