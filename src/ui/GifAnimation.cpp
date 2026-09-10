#include "GifAnimation.hpp"

#include <SDL_image.h>

#include <cstdio>

#include "../platform/EmbeddedAssets.hpp"

namespace rd {

namespace {

constexpr std::uint32_t kMinFrameDelayMs = 20;
constexpr std::uint32_t kDefaultFrameDelayMs = 100;
}

GifAnimation::GifAnimation(SDL_Renderer* renderer, const std::string& path) {
    IMG_Animation* anim = nullptr;
    if (SDL_RWops* rw = Platform::OpenEmbeddedAsset(path)) {
        anim = IMG_LoadAnimation_RW(rw, 1);
    } else {
        anim = IMG_LoadAnimation(path.c_str());
    }
    if (!anim) {
        std::fprintf(stderr, "GifAnimation: failed to load '%s': %s\n", path.c_str(), IMG_GetError());
        return;
    }

    w_ = anim->w;
    h_ = anim->h;
    frames_.reserve(anim->count);
    delays_.reserve(anim->count);

    for (int i = 0; i < anim->count; ++i) {
        SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, anim->frames[i]);
        if (!tex) continue;
        SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
        frames_.push_back(tex);

        std::uint32_t delay = static_cast<std::uint32_t>(anim->delays[i]);
        if (delay < kMinFrameDelayMs) delay = kDefaultFrameDelayMs;
        delays_.push_back(delay);
        totalMs_ += delay;
    }

    IMG_FreeAnimation(anim);
}

GifAnimation::~GifAnimation() {
    for (SDL_Texture* tex : frames_) SDL_DestroyTexture(tex);
}

void GifAnimation::Update(std::uint32_t deltaMs) {
    if (frames_.empty() || finished_) return;

    elapsedInFrameMs_ += deltaMs;
    while (elapsedInFrameMs_ >= delays_[current_]) {
        elapsedInFrameMs_ -= delays_[current_];
        ++current_;
        if (current_ >= frames_.size()) {
            if (loop_) {
                current_ = 0;
            } else {
                current_ = frames_.size() - 1;
                finished_ = true;
                break;
            }
        }
    }
}

SDL_Texture* GifAnimation::CurrentFrame() const {
    return current_ < frames_.size() ? frames_[current_] : nullptr;
}

void GifAnimation::Reset() {
    current_ = 0;
    elapsedInFrameMs_ = 0;
    finished_ = false;
}

} // namespace rd
