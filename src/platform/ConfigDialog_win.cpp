#ifdef _WIN32

#include "ConfigDialog.hpp"

#include <windows.h>

#include <algorithm>
#include <string>

#include "../../resources/resource.h"

namespace rd::Platform {

namespace {

std::wstring ToWide(const std::string& s) {
    if (s.empty()) return {};
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring out(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, out.data(), len);
    out.resize(len - 1);
    return out;
}

std::string ToUtf8(const std::wstring& s) {
    if (s.empty()) return {};
    int len = WideCharToMultiByte(CP_UTF8, 0, s.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string out(len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, s.c_str(), -1, out.data(), len, nullptr, nullptr);
    out.resize(len - 1);
    return out;
}

int GetEditInt(HWND dlg, int id, int fallback, int min, int max) {
    wchar_t buf[32];
    if (GetDlgItemTextW(dlg, id, buf, 32) == 0) return fallback;
    try {
        int value = std::stoi(buf);
        return std::clamp(value, min, max);
    } catch (...) {
        return fallback;
    }
}

std::string GetEditString(HWND dlg, int id) {
    wchar_t buf[512];
    GetDlgItemTextW(dlg, id, buf, 512);
    return ToUtf8(buf);
}

void SetEditInt(HWND dlg, int id, int value) {
    SetDlgItemTextW(dlg, id, std::to_wstring(value).c_str());
}

INT_PTR CALLBACK DialogProc(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    static Config* cfg = nullptr;

    switch (msg) {
        case WM_INITDIALOG: {
            cfg = reinterpret_cast<Config*>(lParam);
            CheckDlgButton(dlg, IDC_SPAWN_AUTO, cfg->spawnAutomatically ? BST_CHECKED : BST_UNCHECKED);
            SetEditInt(dlg, IDC_MIN_DELAY, cfg->minSpawnDelaySec);
            SetEditInt(dlg, IDC_MAX_DELAY, cfg->maxSpawnDelaySec);
            SetEditInt(dlg, IDC_DURATION, cfg->infectionDurationSec);
            SetEditInt(dlg, IDC_AMOUNT, cfg->ransomAmount);
            CheckDlgButton(dlg, IDC_EXEC_CMD, cfg->execCmdOnDeath ? BST_CHECKED : BST_UNCHECKED);
            SetDlgItemTextW(dlg, IDC_CMD, ToWide(cfg->cmdOnDeath).c_str());
            EnableWindow(GetDlgItem(dlg, IDC_MIN_DELAY), cfg->spawnAutomatically);
            EnableWindow(GetDlgItem(dlg, IDC_MAX_DELAY), cfg->spawnAutomatically);
            return TRUE;
        }

        case WM_COMMAND: {
            int id = LOWORD(wParam);
            if (id == IDC_SPAWN_AUTO && HIWORD(wParam) == BN_CLICKED) {
                bool on = IsDlgButtonChecked(dlg, IDC_SPAWN_AUTO) == BST_CHECKED;
                EnableWindow(GetDlgItem(dlg, IDC_MIN_DELAY), on);
                EnableWindow(GetDlgItem(dlg, IDC_MAX_DELAY), on);
                return TRUE;
            }

            if (id == IDOK) {
                bool execChecked = IsDlgButtonChecked(dlg, IDC_EXEC_CMD) == BST_CHECKED;
                std::string cmd = GetEditString(dlg, IDC_CMD);
                if (execChecked) {
                    // Show the literal command, not just "are you sure" - a tampered
                    // or misread field shouldn't be able to sneak this past you.
                    std::wstring prompt = L"This will run the following command if the ransom "
                        L"times out unpaid THIS RUN ONLY (resets off next launch):\n\n" +
                        ToWide(cmd) + L"\n\nEnable it?";
                    int result = MessageBoxW(dlg, prompt.c_str(), L"RANS0M - Run command on death",
                                              MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2);
                    if (result != IDYES) {
                        CheckDlgButton(dlg, IDC_EXEC_CMD, BST_UNCHECKED);
                        return TRUE; // stay open, don't accept the dialog yet
                    }
                }

                cfg->spawnAutomatically = IsDlgButtonChecked(dlg, IDC_SPAWN_AUTO) == BST_CHECKED;
                cfg->minSpawnDelaySec = GetEditInt(dlg, IDC_MIN_DELAY, cfg->minSpawnDelaySec, 0, 86400);
                cfg->maxSpawnDelaySec = GetEditInt(dlg, IDC_MAX_DELAY, cfg->maxSpawnDelaySec, 0, 86400);
                cfg->infectionDurationSec = GetEditInt(dlg, IDC_DURATION, cfg->infectionDurationSec, 5, 86400);
                cfg->ransomAmount = GetEditInt(dlg, IDC_AMOUNT, cfg->ransomAmount, 1, 1000000);
                cfg->execCmdOnDeath = execChecked;
                cfg->cmdOnDeath = cmd;
                EndDialog(dlg, IDOK);
                return TRUE;
            }
            if (id == IDCANCEL) {
                EndDialog(dlg, IDCANCEL);
                return TRUE;
            }
            return FALSE;
        }

        default:
            return FALSE;
    }
}

} // namespace

bool ShowConfigDialog(Config& cfg) {
    INT_PTR result = DialogBoxParamW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDD_CONFIG), nullptr,
                                      DialogProc, reinterpret_cast<LPARAM>(&cfg));
    return result == IDOK;
}

} // namespace rd::Platform

#endif // _WIN32
