#pragma once

#include <SDL.h>
#include <SDL_ttf.h>

#include <string>
#include <unordered_map>
#include <vector>

namespace rd {

// one instance per renderer, textures aren't shareable across renderers
class TextRenderer {
public:
    explicit TextRenderer(SDL_Renderer* renderer) : renderer_(renderer) {}
    ~TextRenderer();

    TextRenderer(const TextRenderer&) = delete;
    TextRenderer& operator=(const TextRenderer&) = delete;

    void LoadFont(std::string path) { fontPath_ = std::move(path); }

    struct Text {
        SDL_Texture* texture = nullptr;
        int w = 0;
        int h = 0;
        int ascent = 0; // baseline offset from top; use for vertical centering, not h/2
    };

    // wrapWidth 0 = no wrap
    Text Get(const std::string& text, int pointSize, SDL_Color color, int wrapWidth = 0);

private:
    TTF_Font* GetFont(int pointSize);

    SDL_Renderer* renderer_;
    std::string fontPath_;
    std::unordered_map<int, TTF_Font*> fonts_;

    struct Entry {
        std::string text;
        int pointSize;
        SDL_Color color;
        int wrapWidth;
        SDL_Texture* texture;
        int w;
        int h;
    };
    std::vector<Entry> cache_;
};

} // namespace rd
