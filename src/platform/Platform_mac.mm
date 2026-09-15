#ifdef __APPLE__

#include "Platform.hpp"

#include <AppKit/AppKit.h>
#include <ApplicationServices/ApplicationServices.h>
#include <Foundation/Foundation.h>
#include <mach-o/dyld.h>
#include <SDL_syswm.h>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <unistd.h>
#include <unordered_set>

#include "../coins/CoinManager.hpp"

namespace rd::Platform {

namespace {

std::unordered_set<SDL_Window*>& TransparentWindows() {
    static std::unordered_set<SDL_Window*> windows;
    return windows;
}

std::unordered_set<SDL_Window*>& AllSpacesWindows() {
    static std::unordered_set<SDL_Window*> windows;
    return windows;
}

NSWindow* NSWindowFor(SDL_Window* window) {
    if (!window) return nil;
    SDL_SysWMinfo info;
    SDL_VERSION(&info.version);
    if (!SDL_GetWindowWMInfo(window, &info)) return nil;
    return info.info.cocoa.window;
}

CFMachPortRef gKeyboardTap = nullptr;
CFRunLoopSourceRef gKeyboardTapSource = nullptr;
std::function<void()> gOnKeyDown;

CGEventRef KeyboardTapCallback(CGEventTapProxy, CGEventType type, CGEventRef event, void*) {
    if (type == kCGEventKeyDown && gOnKeyDown) gOnKeyDown();
    return event; // listen-only tap - must hand the event back through unmodified
}

} // namespace

void RequestPermissions() {
    // pump between prompts so macOS doesn't drop back-to-back ones silently
    auto pumpRunLoop = [] {
        [[NSRunLoop currentRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.3]];
    };

    if (!AXIsProcessTrusted()) {
        NSDictionary* options = @{(__bridge id)kAXTrustedCheckOptionPrompt : @YES};
        AXIsProcessTrustedWithOptions((__bridge CFDictionaryRef)options);
        pumpRunLoop();
    }

    // primes the Automation prompt GetDesktopIconRects() needs
    NSAppleScript* primeAutomation = [[NSAppleScript alloc]
        initWithSource:@"tell application \"Finder\" to get desktop position of every item of desktop"];
    NSDictionary* automationError = nil;
    [primeAutomation executeAndReturnError:&automationError];
    pumpRunLoop();

    // primes per-folder TCC consent for CoinManager's later writes
    const char* home = getenv("HOME");
    if (!home) return;
    std::filesystem::path homeDir(home);
    for (const char* name : kScatterFolderNames) {
        std::filesystem::path probe = homeDir / name / ".rd_permission_probe";
        if (std::ofstream(probe).good()) {
            std::error_code ec;
            std::filesystem::remove(probe, ec);
        }
        pumpRunLoop();
    }
}

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

// real per-pixel alpha, not colorkey - callers must Clear(..., /*a=*/0) for the background here
void MakeWindowColorKeyTransparent(SDL_Window* window, Uint8, Uint8, Uint8) {
    NSWindow* nsWindow = NSWindowFor(window);
    if (!nsWindow) return;

    nsWindow.opaque = NO;
    nsWindow.backgroundColor = [NSColor clearColor];
    nsWindow.hasShadow = NO;

    NSView* contentView = nsWindow.contentView;
    contentView.wantsLayer = YES;
    contentView.layer.opaque = NO;
    contentView.layer.backgroundColor = [NSColor clearColor].CGColor;

    // the renderer's actual CAMetalLayer lives on a subview, not contentView itself
    for (NSView* sub in contentView.subviews) {
        sub.wantsLayer = YES;
        sub.layer.opaque = NO;
        sub.layer.backgroundColor = [NSColor clearColor].CGColor;
    }

    TransparentWindows().insert(window);
}

// SDL2 keeps resetting the layer to opaque black on its own - reassert every frame
void KeepWindowTransparent(SDL_Window* window) {
    if (TransparentWindows().find(window) == TransparentWindows().end()) return;
    if (NSWindow* nsWindow = NSWindowFor(window)) {
        nsWindow.contentView.layer.backgroundColor = [NSColor clearColor].CGColor;
    }
}

void ForgetWindow(SDL_Window* window) {
    TransparentWindows().erase(window);
    AllSpacesWindows().erase(window);
}

void StripWindowButtons(SDL_Window*) {} // needs NSWindow styleMask

// Finder reports this over AppleScript; position is each icon's center, and since it won't give up
// the actual icon size, this just uses a fixed cell close to the macOS default.
std::vector<SDL_Rect> GetDesktopIconRects() {
    std::vector<SDL_Rect> rects;
    constexpr int kIconCellSize = 80;

    NSAppleScript* script = [[NSAppleScript alloc]
        initWithSource:@"tell application \"Finder\" to get desktop position of every item of desktop"];
    NSDictionary* errorInfo = nil;
    NSAppleEventDescriptor* result = [script executeAndReturnError:&errorInfo];
    if (!result) return rects;

    NSInteger count = result.numberOfItems;
    for (NSInteger i = 1; i <= count; ++i) {
        NSAppleEventDescriptor* point = [result descriptorAtIndex:i];
        if (point.numberOfItems < 2) continue;
        int x = [point descriptorAtIndex:1].int32Value;
        int y = [point descriptorAtIndex:2].int32Value;
        rects.push_back(SDL_Rect{x - kIconCellSize / 2, y - kIconCellSize / 2, kIconCellSize, kIconCellSize});
    }
    return rects;
}
SDL_Surface* CaptureDesktopRegion(int, int, int, int) { return nullptr; }
void DisableWindowOpenAnimation(SDL_Window*) {}

// isa-swizzling the window to refuse key/main status crashes SDL's KVO teardown - don't retry that
void MakeWindowNonActivating(SDL_Window*) {}

void MakeWindowClickThrough(SDL_Window* window) {
    if (NSWindow* nsWindow = NSWindowFor(window)) nsWindow.ignoresMouseEvents = YES;
}
// called every frame from IconBlockOverlay - a click can otherwise raise it to the front
void PinWindowToBottom(SDL_Window* window) {
    if (NSWindow* nsWindow = NSWindowFor(window)) [nsWindow orderBack:nil];
}

// no window level bump here (tried NSStatusWindowLevel; it can't reach another app's fullscreen
// Space anyway and just ended up covering system permission dialogs instead)
void ApplyAllSpacesState(NSWindow* nsWindow) {
    nsWindow.collectionBehavior |= NSWindowCollectionBehaviorCanJoinAllSpaces |
                                     NSWindowCollectionBehaviorFullScreenAuxiliary |
                                     NSWindowCollectionBehaviorStationary |
                                     NSWindowCollectionBehaviorIgnoresCycle;
}

void MakeWindowJoinAllSpaces(SDL_Window* window) {
    if (NSWindow* nsWindow = NSWindowFor(window)) {
        ApplyAllSpacesState(nsWindow);
        AllSpacesWindows().insert(window);
    }
}

void KeepWindowInAllSpaces(SDL_Window* window) {
    if (AllSpacesWindows().find(window) == AllSpacesWindows().end()) return;
    if (NSWindow* nsWindow = NSWindowFor(window)) ApplyAllSpacesState(nsWindow);
}
void RestoreIfMinimized(SDL_Window*) {}
void ShowAndForceTopmost(SDL_Window*, int, int, int, int) {}

// requires Accessibility; CGEventTapCreate silently returns NULL without it
void InstallKeyboardHook(std::function<void()> onKeyDown) {
    if (gKeyboardTap) return; // already installed
    gOnKeyDown = std::move(onKeyDown);

    gKeyboardTap = CGEventTapCreate(kCGSessionEventTap, kCGHeadInsertEventTap, kCGEventTapOptionListenOnly,
                                     CGEventMaskBit(kCGEventKeyDown), KeyboardTapCallback, nullptr);
    if (!gKeyboardTap) return;

    gKeyboardTapSource = CFMachPortCreateRunLoopSource(kCFAllocatorDefault, gKeyboardTap, 0);
    CFRunLoopAddSource(CFRunLoopGetMain(), gKeyboardTapSource, kCFRunLoopCommonModes);
    CGEventTapEnable(gKeyboardTap, true);
}

void UninstallKeyboardHook() {
    if (gKeyboardTapSource) {
        CFRunLoopRemoveSource(CFRunLoopGetMain(), gKeyboardTapSource, kCFRunLoopCommonModes);
        CFRelease(gKeyboardTapSource);
        gKeyboardTapSource = nullptr;
    }
    if (gKeyboardTap) {
        CGEventTapEnable(gKeyboardTap, false);
        CFRelease(gKeyboardTap);
        gKeyboardTap = nullptr;
    }
    gOnKeyDown = nullptr;
}

// no reliable way to do this found yet (defaults-write to System Settings' pointer color is blocked;
// a mouse-following overlay window is laggy; the private CGS cursor-registration SPI works but won't
// restore reliably) - left as a no-op rather than risk a stuck system cursor
void SetInfectedCursor(const std::filesystem::path&) {}
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
