#include "EmbeddedAssets.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cwctype>

#ifdef _WIN32
#include <windows.h>
#endif

namespace rd::Platform {

namespace {

std::string RelativeAssetKey(const std::string& path) {
    std::string normalized = path;
    std::replace(normalized.begin(), normalized.end(), '\\', '/');

    std::string lower = normalized;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                    [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    std::size_t pos = lower.rfind("assets/");
    if (pos == std::string::npos) return normalized; // already bare
    return normalized.substr(pos + 7);                // skip "assets/"
}

#ifdef _WIN32
std::wstring ToWide(const std::string& s) {
    if (s.empty()) return {};
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring out(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, out.data(), len);
    out.resize(len - 1);
    return out;
}

BOOL CALLBACK CaptureFirstLangId(HMODULE, LPCWSTR, LPCWSTR, WORD langId, LONG_PTR param) {
    *reinterpret_cast<WORD*>(param) = langId;
    return FALSE; // stop after the first one
}

bool FindEmbedded(const std::string& path, const void** data, size_t* size) {
    std::wstring name = ToWide(RelativeAssetKey(path));
    std::transform(name.begin(), name.end(), name.begin(),
                    [](wchar_t c) { return static_cast<wchar_t>(std::towupper(c)); });
    name = L"\"" + name + L"\"";
    HMODULE self = GetModuleHandleW(nullptr);
    HRSRC res = FindResourceExW(self, L"ASSET", name.c_str(), MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL));
    if (!res) {
        WORD langId = 0xFFFF;
        EnumResourceLanguagesW(self, L"ASSET", name.c_str(),
                                reinterpret_cast<ENUMRESLANGPROCW>(&CaptureFirstLangId),
                                reinterpret_cast<LONG_PTR>(&langId));
        if (langId != 0xFFFF) res = FindResourceExW(self, L"ASSET", name.c_str(), langId);
    }
    if (!res) return false;
    HGLOBAL loaded = LoadResource(self, res);
    if (!loaded) return false;
    *data = LockResource(loaded);
    *size = SizeofResource(self, res);
    return *data != nullptr && *size > 0;
}
#endif

} // namespace

SDL_RWops* OpenEmbeddedAsset(const std::string& path) {
#ifdef _WIN32
    const void* data = nullptr;
    size_t size = 0;
    if (!FindEmbedded(path, &data, &size)) return nullptr;
    return SDL_RWFromConstMem(data, static_cast<int>(size));
#else
    (void)path;
    return nullptr;
#endif
}

bool GetEmbeddedAssetBytes(const std::string& path, const void** data, size_t* size) {
#ifdef _WIN32
    return FindEmbedded(path, data, size);
#else
    (void)path;
    (void)data;
    (void)size;
    return false;
#endif
}

} // namespace rd::Platform
