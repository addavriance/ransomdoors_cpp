#ifdef __APPLE__

#include "Platform.hpp"

#include <AppKit/AppKit.h>
#include <Foundation/Foundation.h>
#include <mach-o/dyld.h>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

namespace rd::Platform {

bool IsAdmin() {
    return geteuid() == 0;
}

// no BSOD equivalent, real shutdown either way
void RealHardShutdown() {
    if (IsAdmin()) {
        system("/sbin/shutdown -h now");
        return;
    }

    NSString* script = @"do shell script \"/sbin/shutdown -h now\" with administrator privileges";
    NSAppleScript* appleScript = [[NSAppleScript alloc] initWithSource:script];
    NSDictionary* errorInfo = nil;
    [appleScript executeAndReturnError:&errorInfo];
}

void RunOnDeathCommand(const std::string& cmd) {
    if (!cmd.empty()) system(cmd.c_str());
}

void RegisterFileTypeIcon(const std::string&, const std::filesystem::path&) {} // needs Launch Services/UTType

void MakeWindowColorKeyTransparent(SDL_Window*, Uint8, Uint8, Uint8) {} // needs NSWindow clear bg

void StripWindowButtons(SDL_Window*) {} // needs NSWindow styleMask

std::vector<SDL_Rect> GetDesktopIconRects() { return {}; } // no Finder equivalent API
SDL_Surface* CaptureDesktopRegion(int, int, int, int) { return nullptr; }
void DisableWindowOpenAnimation(SDL_Window*) {}
void MakeWindowNonActivating(SDL_Window*) {}
void MakeWindowClickThrough(SDL_Window*) {} // needs NSWindow ignoresMouseEvents
void PinWindowToBottom(SDL_Window*) {}
void RestoreIfMinimized(SDL_Window*) {}
void ShowAndForceTopmost(SDL_Window*, int, int, int, int) {}
void InstallKeyboardHook(std::function<void()>) {}
void UninstallKeyboardHook() {}
void SetInfectedCursor(const std::filesystem::path&) {} // needs NSCursor
void RestoreCursor() {}

// pgrep/kill, unverified
void KillOtherInstances() {
    char ownPath[1024] = {0};
    uint32_t size = sizeof(ownPath);
    if (_NSGetExecutablePath(ownPath, &size) != 0) return;
    const char* ownName = strrchr(ownPath, '/');
    ownName = ownName ? ownName + 1 : ownPath;

    char cmd[1200];
    snprintf(cmd, sizeof(cmd), "pgrep -x '%s'", ownName);
    FILE* pipe = popen(cmd, "r");
    if (!pipe) return;

    pid_t ownPid = getpid();
    char line[64];
    while (fgets(line, sizeof(line), pipe)) {
        pid_t pid = static_cast<pid_t>(atoi(line));
        if (pid > 0 && pid != ownPid) kill(pid, SIGKILL);
    }
    pclose(pipe);
}

void OpenFolder(const std::filesystem::path& folder) {
    [[NSWorkspace sharedWorkspace] openURL:[NSURL fileURLWithPath:@(folder.string().c_str())]];
}

} // namespace rd::Platform

#endif // __APPLE__
