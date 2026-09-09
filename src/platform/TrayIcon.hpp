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
    void SetHardmodeChecked(bool checked);

    void Pump();

    std::function<void()> onClose;
    std::function<void()> onToggleHardmode;

private:
    struct Impl;
    Impl* impl_;
};

} // namespace rd
