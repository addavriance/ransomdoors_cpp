#ifdef __APPLE__

#include "TrayIcon.hpp"

namespace rd {

// NSStatusBar/NSMenu port not done yet - no tray icon means no way to quit on macOS currently
struct TrayIcon::Impl {};

TrayIcon::TrayIcon(const std::string&) : impl_(new Impl()) {}
TrayIcon::~TrayIcon() { delete impl_; }
void TrayIcon::SetCloseEnabled(bool) {}
void TrayIcon::Pump() {}

} // namespace rd

#endif // __APPLE__
