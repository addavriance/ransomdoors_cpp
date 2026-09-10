#pragma once

#include <functional>
#include <string>

namespace rd {

class TrayIcon {
public:
    explicit TrayIcon(const std::string& tooltip);
    ~TrayIcon();

    TrayIcon(const TrayIcon&) = delete;
    TrayIcon& operator=(const TrayIcon&) = delete;

    void SetCloseEnabled(bool enabled);

    void Pump();

    std::function<void()> onClose;
    std::function<void()> onOpenConfig;

private:
    struct Impl;
    Impl* impl_;
};

} // namespace rd
