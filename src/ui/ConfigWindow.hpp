#pragma once

#include <functional>
#include <memory>
#include <string>

#include <SDL.h>

#include "../config/Config.hpp"

namespace rd {

struct ConfigWindowAssets {
    std::string fontPath;
    std::string starlightPath;
};

class ConfigWindow {
public:
    ConfigWindow();
    ~ConfigWindow();
    ConfigWindow(const ConfigWindow&) = delete;
    ConfigWindow& operator=(const ConfigWindow&) = delete;

    static bool ShowModal(Config& cfg, const ConfigWindowAssets& assets, bool* spawnRequested = nullptr);

    void Open(const Config& initial, const ConfigWindowAssets& assets);
    void HandleEvent(const SDL_Event& e);
    void Render();

    void Close();

    std::function<void(const Config& cfg, bool spawnRequested)> onAccepted;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace rd
