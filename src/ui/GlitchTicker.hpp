#pragma once

#include <cstdint>
#include <cstdlib>

namespace rd {

// jitters in place, no drift
class GlitchTicker {
public:
    void SetBase(int x, int y) { baseX_ = x; baseY_ = y; }
    int BaseX() const { return baseX_; }
    int BaseY() const { return baseY_; }
    int X() const { return baseX_ + jitterX_; }
    int Y() const { return baseY_ + jitterY_; }

    // true only when jitter recomputed
    bool Tick(std::uint32_t deltaMs) {
        accMs_ += deltaMs;
        if (accMs_ < kTickMs) return false;
        accMs_ -= kTickMs;
        jitterX_ = (std::rand() % 11) - 5;
        jitterY_ = (std::rand() % 11) - 5;
        return true;
    }

private:
    static constexpr std::uint32_t kTickMs = 200;
    int baseX_ = 0;
    int baseY_ = 0;
    int jitterX_ = 0;
    int jitterY_ = 0;
    std::uint32_t accMs_ = 0;
};

} // namespace rd
