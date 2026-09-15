#include "Window.hpp"

#include <cstdio>

#include "../platform/EmbeddedAssets.hpp"
#include "../platform/Platform.hpp"

namespace rd {

Window::Window(const std::string& title, int w, int h, Uint32 extraFlags, bool startVisible, bool titleBar,
               bool softwareRenderer) {
    Uint32 visibilityFlag = startVisible ? SDL_WINDOW_SHOWN : SDL_WINDOW_HIDDEN; // avoids launch flash
    Uint32 chromeFlag = titleBar ? 0 : SDL_WINDOW_BORDERLESS;
    window_ = SDL_CreateWindow(title.c_str(), SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, w, h,
                                visibilityFlag | chromeFlag | extraFlags);
    if (!window_) return;
    Uint32 rendererFlags = softwareRenderer ? SDL_RENDERER_SOFTWARE : (SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    renderer_ = SDL_CreateRenderer(window_, -1, rendererFlags);
    if (titleBar) Platform::StripWindowButtons(window_);
    if (extraFlags & SDL_WINDOW_ALWAYS_ON_TOP) Platform::MakeWindowJoinAllSpaces(window_);
}

Window::~Window() { Destroy(); }

Window::Window(Window&& other) noexcept : window_(other.window_), renderer_(other.renderer_) {
    other.window_ = nullptr;
    other.renderer_ = nullptr;
}

Window& Window::operator=(Window&& other) noexcept {
    if (this != &other) {
        Destroy();
        window_ = other.window_;
        renderer_ = other.renderer_;
        other.window_ = nullptr;
        other.renderer_ = nullptr;
    }
    return *this;
}

void Window::Destroy() {
    if (renderer_) SDL_DestroyRenderer(renderer_);
    if (window_) {
        Platform::ForgetWindow(window_);
        SDL_DestroyWindow(window_);
    }
    renderer_ = nullptr;
    window_ = nullptr;
}

Uint32 Window::Id() const {
    return window_ ? SDL_GetWindowID(window_) : 0;
}

void Window::Show() { if (window_) SDL_ShowWindow(window_); }
void Window::Hide() { if (window_) SDL_HideWindow(window_); }

void Window::SetPosition(int x, int y) {
    if (window_) SDL_SetWindowPosition(window_, x, y);
}

void Window::SetSize(int w, int h) {
    if (window_) SDL_SetWindowSize(window_, w, h);
}

SDL_Point Window::Position() const {
    SDL_Point p{0, 0};
    if (window_) SDL_GetWindowPosition(window_, &p.x, &p.y);
    return p;
}

void Window::Clear(Uint8 r, Uint8 g, Uint8 b, Uint8 a) {
    SDL_SetRenderDrawColor(renderer_, r, g, b, a);
    SDL_RenderClear(renderer_);
}

void Window::Present() {
    Platform::KeepWindowTransparent(window_); // no-op on Windows
    Platform::KeepWindowInAllSpaces(window_); // no-op on Windows
    SDL_RenderPresent(renderer_);
}

SDL_Texture* Window::LoadTexture(const std::string& path) {
    SDL_Texture* tex = nullptr;
    if (SDL_RWops* rw = Platform::OpenEmbeddedAsset(path)) {
        tex = IMG_LoadTexture_RW(renderer_, rw, 1);
    } else {
        tex = IMG_LoadTexture(renderer_, path.c_str());
    }
    if (!tex) std::fprintf(stderr, "Window::LoadTexture: failed to load '%s': %s\n", path.c_str(), IMG_GetError());
    return tex;
}

} // namespace rd
