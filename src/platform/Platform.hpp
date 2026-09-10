#pragma once

#include <SDL.h>

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace rd {

// RealHardShutdown gated by Config::crashOnDeath (session-only, see Config.hpp).
namespace Platform {

bool IsAdmin();

// BSOD if elevated, else real shutdown.
void RealHardShutdown();

void RunOnDeathCommand(const std::string& cmd);

// extension e.g. ".gold1"; no-op on macOS
void RegisterFileTypeIcon(const std::string& extension, const std::filesystem::path& icoSource);

// WS_EX_LAYERED colorkey; no-op on macOS
void MakeWindowColorKeyTransparent(SDL_Window* window, Uint8 r, Uint8 g, Uint8 b);

// strips sysmenu/min/max, keeps title bar
void StripWindowButtons(SDL_Window* window);

// WS_EX_NOACTIVATE + TOOLWINDOW; no-op on macOS
void MakeWindowNonActivating(SDL_Window* window);

// WS_EX_TRANSPARENT - mouse/click events pass through to whatever's behind this window; no-op on macOS
void MakeWindowClickThrough(SDL_Window* window);

// pins to HWND_BOTTOM; no-op on macOS
void PinWindowToBottom(SDL_Window* window);

// reactive Win+D un-minimize; no-op on macOS
void RestoreIfMinimized(SDL_Window* window);

// atomic pos+size+show+topmost, avoids race; no-op on macOS
void ShowAndForceTopmost(SDL_Window* window, int x, int y, int w, int h);

// kills other instances by exe name, call before touching shared config
void KillOtherInstances();

// via SysListView32; empty on macOS
std::vector<SDL_Rect> GetDesktopIconRects();

// unused fallback (see IconBlockOverlay); no-op on macOS
SDL_Surface* CaptureDesktopRegion(int x, int y, int w, int h);

// DWMWA_TRANSITIONS_FORCEDISABLED; no-op on macOS
void DisableWindowOpenAnimation(SDL_Window* window);

// system-wide WH_KEYBOARD_LL hook; no-op on macOS
void InstallKeyboardHook(std::function<void()> onKeyDown);
void UninstallKeyboardHook();

// SetSystemCursor(OCR_NORMAL); system takes ownership of the cursor handle; no-op on macOS
void SetInfectedCursor(const std::filesystem::path& curSource);
// resets to the user's configured scheme (not a true per-app undo)
void RestoreCursor();

// opens a folder in Explorer/Finder
void OpenFolder(const std::filesystem::path& folder);

} // namespace Platform
} // namespace rd
