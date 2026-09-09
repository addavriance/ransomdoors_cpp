#include "Text.hpp"

namespace rd {

TextRenderer::~TextRenderer() {
    for (auto& entry : cache_) {
        if (entry.texture) SDL_DestroyTexture(entry.texture);
    }
    for (auto& [size, font] : fonts_) {
        if (font) TTF_CloseFont(font);
    }
}

TTF_Font* TextRenderer::GetFont(int pointSize) {
    auto it = fonts_.find(pointSize);
    if (it != fonts_.end()) return it->second;

    TTF_Font* font = TTF_OpenFont(fontPath_.c_str(), pointSize);
    if (font) TTF_SetFontWrappedAlign(font, TTF_WRAPPED_ALIGN_CENTER); // per-line centering, not just block
    fonts_[pointSize] = font;
    return font;
}

TextRenderer::Text TextRenderer::Get(const std::string& text, int pointSize, SDL_Color color, int wrapWidth) {
    TTF_Font* font = GetFont(pointSize);
    if (!font) return {};
    int ascent = TTF_FontAscent(font);

    for (const auto& entry : cache_) {
        if (entry.text == text && entry.pointSize == pointSize && entry.wrapWidth == wrapWidth &&
            entry.color.r == color.r && entry.color.g == color.g && entry.color.b == color.b &&
            entry.color.a == color.a) {
            return {entry.texture, entry.w, entry.h, ascent};
        }
    }

    SDL_Surface* surface = wrapWidth > 0
        ? TTF_RenderUTF8_Blended_Wrapped(font, text.c_str(), color, static_cast<Uint32>(wrapWidth))
        : TTF_RenderUTF8_Blended(font, text.c_str(), color);
    if (!surface) return {};

    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer_, surface);
    int w = surface->w;
    int h = surface->h;
    SDL_FreeSurface(surface);
    if (!texture) return {};

    cache_.push_back(Entry{text, pointSize, color, wrapWidth, texture, w, h});
    return {texture, w, h, ascent};
}

} // namespace rd
