#pragma once

#include <SDL.h>

#include <cstdint>
#include <string>
#include <vector>

namespace rd {

class GifAnimation {
public:
    GifAnimation(SDL_Renderer* renderer, const std::string& path);
    ~GifAnimation();

    GifAnimation(const GifAnimation&) = delete;
    GifAnimation& operator=(const GifAnimation&) = delete;

    bool Valid() const { return !frames_.empty(); }

    void SetLoop(bool loop) { loop_ = loop; }
    void Update(std::uint32_t deltaMs);
    void Reset(); // rewind to frame 0, doesn't re-decode

    SDL_Texture* CurrentFrame() const;
    int Width() const { return w_; }
    int Height() const { return h_; }
    bool Finished() const { return finished_; } // only meaningful with SetLoop(false)
    std::uint32_t TotalDurationMs() const { return totalMs_; }

private:
    std::vector<SDL_Texture*> frames_;
    std::vector<std::uint32_t> delays_;
    int w_ = 0;
    int h_ = 0;
    std::uint32_t totalMs_ = 0;

    std::size_t current_ = 0;
    std::uint32_t elapsedInFrameMs_ = 0;
    bool loop_ = true;
    bool finished_ = false;
};

} // namespace rd
