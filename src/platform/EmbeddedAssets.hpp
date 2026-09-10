#pragma once

#include <string>

#include <SDL.h>

namespace rd::Platform {

SDL_RWops* OpenEmbeddedAsset(const std::string& path);

bool GetEmbeddedAssetBytes(const std::string& path, const void** data, size_t* size);

} // namespace rd::Platform
