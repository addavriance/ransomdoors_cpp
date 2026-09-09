#ifdef _WIN32

#include "TrayIcon.hpp"

#include <windows.h>
#include <shellapi.h>

namespace rd {

namespace {
constexpr UINT kCallbackMsg = WM_APP + 1;
constexpr UINT kIdClose = 1;
constexpr UINT kIdHardmode = 2;
constexpr UINT kIdConfig = 3;
constexpr wchar_t kClassName[] = L"RansomdoorsTrayWnd";

std::wstring ToWide(const std::string& s) {
    if (s.empty()) return {};
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring out(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, out.data(), len);
    out.resize(len - 1); // drop the trailing NUL MultiByteToWideChar counted
    return out;
}
} // namespace

struct TrayIcon::Impl {
    HWND hwnd = nullptr;
    NOTIFYICONDATAW nid{};
    HICON icon = nullptr;
    bool closeEnabled = true;
    bool hardmodeChecked = false;
    TrayIcon* self = nullptr;

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        auto* impl = reinterpret_cast<Impl*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (msg == kCallbackMsg && impl) {
            UINT event = LOWORD(lParam);
            if (event == WM_RBUTTONUP || event == WM_LBUTTONUP) {
                POINT pt;
                GetCursorPos(&pt);

                HMENU menu = CreatePopupMenu();
                AppendMenuW(menu, MF_STRING | (impl->closeEnabled ? MF_ENABLED : MF_GRAYED), kIdClose, L"Close");
                AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
                AppendMenuW(menu, MF_STRING | (impl->hardmodeChecked ? MF_CHECKED : MF_UNCHECKED), kIdHardmode,
                            L"Hardmode");
                AppendMenuW(menu, MF_STRING, kIdConfig, L"Config...");

                SetForegroundWindow(hwnd); // required or the menu won't dismiss on outside click
                TrackPopupMenu(menu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, nullptr);
                DestroyMenu(menu);
            }
            return 0;
        }
        if (msg == WM_COMMAND && impl) {
            UINT id = LOWORD(wParam);
            if (id == kIdClose && impl->closeEnabled && impl->self->onClose) impl->self->onClose();
            if (id == kIdHardmode && impl->self->onToggleHardmode) impl->self->onToggleHardmode();
            if (id == kIdConfig && impl->self->onOpenConfig) impl->self->onOpenConfig();
            return 0;
        }
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
};

TrayIcon::TrayIcon(const std::string& tooltip) : impl_(new Impl()) {
    impl_->self = this;

    static bool classRegistered = false;
    if (!classRegistered) {
        WNDCLASSW wc{};
        wc.lpfnWndProc = Impl::WndProc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.lpszClassName = kClassName;
        RegisterClassW(&wc);
        classRegistered = true;
    }

    // A real (if invisible) top-level window, not HWND_MESSAGE - SetForegroundWindow
    // (needed so the popup menu dismisses on an outside click) requires one.
    impl_->hwnd = CreateWindowW(kClassName, L"", WS_POPUP, 0, 0, 0, 0, nullptr, nullptr, GetModuleHandleW(nullptr),
                                 nullptr);
    if (!impl_->hwnd) return;
    SetWindowLongPtrW(impl_->hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(impl_));

    impl_->icon = LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(101)); // resources/app.rc's icon ID

    impl_->nid.cbSize = sizeof(NOTIFYICONDATAW);
    impl_->nid.hWnd = impl_->hwnd;
    impl_->nid.uID = 1;
    impl_->nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    impl_->nid.uCallbackMessage = kCallbackMsg;
    impl_->nid.hIcon = impl_->icon;
    std::wstring wideTooltip = ToWide(tooltip);
    wcsncpy_s(impl_->nid.szTip, wideTooltip.c_str(), _TRUNCATE);
    Shell_NotifyIconW(NIM_ADD, &impl_->nid);
}

TrayIcon::~TrayIcon() {
    if (impl_->hwnd) {
        Shell_NotifyIconW(NIM_DELETE, &impl_->nid);
        DestroyWindow(impl_->hwnd);
    }
    // icon came from LoadIconW (module resource, shared) - not ours to DestroyIcon.
    delete impl_;
}

void TrayIcon::SetCloseEnabled(bool enabled) { impl_->closeEnabled = enabled; }
void TrayIcon::SetHardmodeChecked(bool checked) { impl_->hardmodeChecked = checked; }

void TrayIcon::Pump() {
    if (!impl_->hwnd) return;
    MSG msg;
    while (PeekMessageW(&msg, impl_->hwnd, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
}

} // namespace rd

#endif // _WIN32
