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

// primes macOS TCC prompts (Accessibility, Automation, folder access) up front; no-op on Windows
void RequestPermissions();

// BSOD if elevated, else real shutdown.
void RealHardShutdown();

void RunOnDeathCommand(const std::string& cmd);

// extension e.g. ".gold1"; no-op on macOS
void RegisterFileTypeIcon(const std::string& extension, const std::filesystem::path& icoSource);

// WS_EX_LAYERED colorkey on Windows; real alpha transparency on macOS (clear with alpha 0 there)
void MakeWindowColorKeyTransparent(SDL_Window* window, Uint8 r, Uint8 g, Uint8 b);

// macOS only: call every frame for windows passed to MakeWindowColorKeyTransparent (SDL2 keeps
// resetting the transparency on its own)
void KeepWindowTransparent(SDL_Window* window);

// call once from Window::~Window(); no-op on Windows
void ForgetWindow(SDL_Window* window);

// strips sysmenu/min/max, keeps title bar
void StripWindowButtons(SDL_Window* window);

// WS_EX_NOACTIVATE + TOOLWINDOW; no-op on macOS (isa-swizzling this crashes SDL's KVO teardown)
void MakeWindowNonActivating(SDL_Window* window);

// WS_EX_TRANSPARENT - clicks pass through to whatever's behind; NSWindow.ignoresMouseEvents on macOS
void MakeWindowClickThrough(SDL_Window* window);

// pins to HWND_BOTTOM; on macOS via [NSWindow orderBack:]
void PinWindowToBottom(SDL_Window* window);

// follows the user onto every macOS Space; no-op on Windows (HWND_TOPMOST already covers this there)
void MakeWindowJoinAllSpaces(SDL_Window* window);

// macOS only: call every frame for windows passed to MakeWindowJoinAllSpaces
void KeepWindowInAllSpaces(SDL_Window* window);

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

// system-wide WH_KEYBOARD_LL hook; on macOS via a listen-only CGEventTap (requires Accessibility)
void InstallKeyboardHook(std::function<void()> onKeyDown);
void UninstallKeyboardHook();

// SetSystemCursor(OCR_NORMAL); no-op on macOS (see Platform_mac.mm - no reliable approach found)
void SetInfectedCursor(const std::filesystem::path& curSource);
// resets to the user's configured scheme (not a true per-app undo)
void RestoreCursor();

// opens a folder in Explorer/Finder
void OpenFolder(const std::filesystem::path& folder);

} // namespace Platform
} // namespace rd
