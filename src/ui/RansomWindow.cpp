#include "RansomWindow.hpp"

#include <cstdio>
#include <cstdlib>

namespace rd {

namespace {
constexpr int kWidth = 544;
constexpr int kHeight = 315;

constexpr SDL_Color kWhite{255, 255, 255, 255};
constexpr SDL_Color kBlack{0, 0, 0, 255};
constexpr SDL_Color kGold{255, 215, 0, 255};
constexpr SDL_Color kRed{255, 0, 0, 255};
constexpr SDL_Color kBorderGold{255, 207, 37, 255};

void DrawBox(SDL_Renderer* renderer, SDL_Rect rect, Uint8 r, Uint8 g, Uint8 b) {
    SDL_SetRenderDrawColor(renderer, r, g, b, 255);
    SDL_RenderFillRect(renderer, &rect);
}

void DrawBorder(SDL_Renderer* renderer, SDL_Rect rect, SDL_Color color, int thickness) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    for (int i = 0; i < thickness; ++i) {
        SDL_Rect r{rect.x + i, rect.y + i, rect.w - 2 * i, rect.h - 2 * i};
        SDL_RenderDrawRect(renderer, &r);
    }
}

// wrapped text centers on full block height; single-line centers on ascent (not h/2), since unused descender space otherwise reads as "too high"
void DrawTextCentered(TextRenderer& text, SDL_Renderer* renderer, const std::string& s, int pointSize,
                       SDL_Color color, SDL_Rect box, int wrapWidth) {
    TextRenderer::Text t = text.Get(s, pointSize, color, wrapWidth);
    if (!t.texture) return;
    int y = wrapWidth > 0 ? box.y + (box.h - t.h) / 2 : box.y + (box.h - t.ascent) / 2;
    SDL_Rect dst{box.x + (box.w - t.w) / 2, y, t.w, t.h};
    SDL_RenderSetClipRect(renderer, &box); // WinForms Label clips to its bounds; we don't by default
    SDL_RenderCopy(renderer, t.texture, nullptr, &dst);
    SDL_RenderSetClipRect(renderer, nullptr);
}

void DrawTextLeft(TextRenderer& text, SDL_Renderer* renderer, const std::string& s, int pointSize,
                   SDL_Color color, SDL_Rect box) {
    TextRenderer::Text t = text.Get(s, pointSize, color, 0);
    if (!t.texture) return;
    SDL_Rect dst{box.x + 8, box.y + (box.h - t.ascent) / 2, t.w, t.h};
    SDL_RenderSetClipRect(renderer, &box);
    SDL_RenderCopy(renderer, t.texture, nullptr, &dst);
    SDL_RenderSetClipRect(renderer, nullptr);
}

void DrawTwoColorParagraph(TextRenderer& text, SDL_Renderer* renderer, const std::string& part1, SDL_Color color1,
                            const std::string& part2, SDL_Color color2, int pointSize, SDL_Rect box, int wrapWidth) {
    TextRenderer::Text t1 = text.Get(part1, pointSize, color1, wrapWidth);
    TextRenderer::Text t2 = text.Get(part2, pointSize, color2, wrapWidth);
    int totalH = t1.h + t2.h;
    int y = box.y + (box.h - totalH) / 2;
    SDL_RenderSetClipRect(renderer, &box);
    if (t1.texture) {
        SDL_Rect dst{box.x + (box.w - t1.w) / 2, y, t1.w, t1.h};
        SDL_RenderCopy(renderer, t1.texture, nullptr, &dst);
    }
    if (t2.texture) {
        SDL_Rect dst{box.x + (box.w - t2.w) / 2, y + t1.h, t2.w, t2.h};
        SDL_RenderCopy(renderer, t2.texture, nullptr, &dst);
    }
    SDL_RenderSetClipRect(renderer, nullptr);
}
} // namespace

RansomWindow::RansomWindow(int screenW, int screenH, CoinManager& coins, const std::string& ransomIdlePath,
                            const std::string& goldPath, const std::string& fontPath, int ransomAmount)
    : window_("RANS0M", kWidth, kHeight, SDL_WINDOW_ALWAYS_ON_TOP | SDL_WINDOW_SKIP_TASKBAR,
              /*startVisible=*/true, /*titleBar=*/true),
      coins_(coins),
      text_(window_.Renderer()),
      screenW_(screenW),
      screenH_(screenH),
      ransomLeft_(ransomAmount) {
    if (!window_.Valid()) return;
    int x = std::rand() % (screenW - kWidth > 0 ? screenW - kWidth : 1);
    int y = std::rand() % (screenH - kHeight > 0 ? screenH - kHeight : 1);
    ticker_.SetBase(x, y);
    window_.SetPosition(x, y);
    SDL_SetWindowGrab(window_.Raw(), SDL_FALSE);

    text_.LoadFont(fontPath);
    texRansomIdle_ = window_.LoadTexture(ransomIdlePath);
    texGold_ = window_.LoadTexture(goldPath);
}

void RansomWindow::HandleDropFile(const std::filesystem::path& path) {
    RedeemResult result = coins_.TryRedeem(path);
    if (!result.ok) return;

    if (result.isCrucifix) {
        ransomLeft_ = 0;
        if (onCrucifix) onCrucifix();
    } else {
        if (onCoinRedeemed) onCoinRedeemed();
        ransomLeft_ -= result.value;
    }
    if (ransomLeft_ <= 0 && onFullyPaid) onFullyPaid();
}

void RansomWindow::Update(std::uint32_t deltaMs) {
    if (!ticker_.Tick(deltaMs)) return;

    SDL_SetWindowAlwaysOnTop(window_.Raw(), SDL_TRUE); // doesn't stick otherwise

    if (std::rand() % 100 < 2) {
        int maxX = screenW_ - kWidth > 0 ? screenW_ - kWidth : 1;
        int maxY = screenH_ - kHeight > 0 ? screenH_ - kHeight : 1;
        ticker_.SetBase(std::rand() % maxX, std::rand() % maxY);
        if (onWantsMoreTaunt) onWantsMoreTaunt();
    }
    window_.SetPosition(ticker_.X(), ticker_.Y());
}

void RansomWindow::Render(std::uint32_t remainingMs) {
    SDL_Renderer* renderer = window_.Renderer();
    window_.Clear(255, 0, 0);

    if (texRansomIdle_) {
        SDL_Rect dst{28, 12, 188, 185};
        SDL_RenderCopy(renderer, texRansomIdle_, nullptr, &dst);
    }

    DrawTextCentered(text_, renderer, "YOUR FILES HAVE BEEN ENCRYPTED", 36, kWhite, SDL_Rect{233, 9, 267, 154}, 267);

    SDL_Rect noteBox{12, 166, 520, 76};
    DrawBox(renderer, noteBox, 0, 0, 0);
    DrawTwoColorParagraph(text_, renderer, "IF YOU DO NOT PAY THIS RANSOM BY THE END OF THE TIMER, YOUR FILES WILL BE",
                           kWhite, "UNRECOVERABLE BY ANY MEANS.", kRed, 21, noteBox, 500);
    DrawBorder(renderer, noteBox, kWhite, 2);

    std::uint32_t totalSeconds = remainingMs / 1000;
    std::uint32_t minutes = totalSeconds / 60;
    std::uint32_t seconds = totalSeconds % 60;
    char timeBuf[16];
    std::snprintf(timeBuf, sizeof(timeBuf), "TIME: %02u:%02u", minutes, seconds);
    SDL_Rect timeBox{206, 245, 326, 61};
    DrawTextCentered(text_, renderer, timeBuf, 48, kBlack, timeBox, 0);
    DrawBorder(renderer, timeBox, kWhite, 2);

    SDL_Rect cashBox{12, 246, 188, 61};
    DrawBox(renderer, cashBox, 0, 0, 0);
    int displayCash = ransomLeft_ > 0 ? ransomLeft_ : 0;
    DrawTextLeft(text_, renderer, std::to_string(displayCash), 48, kGold, cashBox);

    SDL_Rect goldBox{134, 246, 66, 61};
    DrawBox(renderer, goldBox, 0, 0, 0);
    if (texGold_) SDL_RenderCopy(renderer, texGold_, nullptr, &goldBox);

    DrawBorder(renderer, cashBox, kBorderGold, 2);

    window_.Present();
}

} // namespace rd
