#pragma once

#include <SDL.h>
#include <SDL_image.h>

#include <string>

namespace rd {

class Window {
public:
    // titleBar keeps a native title bar, buttons stripped
    Window(const std::string& title, int w, int h, Uint32 extraFlags = 0, bool startVisible = true,
           bool titleBar = false, bool softwareRenderer = false);
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    Window(Window&&) noexcept;
    Window& operator=(Window&&) noexcept;

    bool Valid() const { return window_ != nullptr && renderer_ != nullptr; }
    Uint32 Id() const;

    SDL_Window* Raw() { return window_; }
    SDL_Renderer* Renderer() { return renderer_; }

    void Show();
    void Hide();
    void SetPosition(int x, int y);
    void Clear(Uint8 r, Uint8 g, Uint8 b, Uint8 a = 255);
    void Present();

    SDL_Texture* LoadTexture(const std::string& path);

private:
    void Destroy();

    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
};

} // namespace rd
