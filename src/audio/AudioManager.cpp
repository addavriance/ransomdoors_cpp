#include "AudioManager.hpp"

#include "../platform/EmbeddedAssets.hpp"

namespace rd {

bool AudioManager::Init() {
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 1024) < 0) return false;
    Mix_AllocateChannels(32);
    return true;
}

void AudioManager::Shutdown() {
    for (auto& [key, chunk] : sfx_) Mix_FreeChunk(chunk);
    for (auto& [key, track] : music_) Mix_FreeMusic(track);
    sfx_.clear();
    music_.clear();
    Mix_CloseAudio();
}

void AudioManager::LoadMusic(std::string_view key, const std::string& path) {
    Mix_Music* track = nullptr;
    if (SDL_RWops* rw = Platform::OpenEmbeddedAsset(path)) {
        track = Mix_LoadMUS_RW(rw, 1);
    } else {
        track = Mix_LoadMUS(path.c_str());
    }
    if (track) music_[std::string(key)] = track;
}

void AudioManager::LoadSfx(std::string_view key, const std::string& path) {
    Mix_Chunk* chunk = nullptr;
    if (SDL_RWops* rw = Platform::OpenEmbeddedAsset(path)) {
        chunk = Mix_LoadWAV_RW(rw, 1);
    } else {
        chunk = Mix_LoadWAV(path.c_str());
    }
    if (chunk) sfx_[std::string(key)] = chunk;
}

void AudioManager::PlayMusicLoop(std::string_view key) {
    auto it = music_.find(std::string(key));
    if (it == music_.end()) return;
    Mix_PlayMusic(it->second, -1);
}

void AudioManager::StopMusic() {
    Mix_HaltMusic();
}

void AudioManager::PlaySfx(std::string_view key, float volume) {
    auto it = sfx_.find(std::string(key));
    if (it == sfx_.end()) return;
    Mix_VolumeChunk(it->second, static_cast<int>(volume * MIX_MAX_VOLUME));
    Mix_PlayChannel(-1, it->second, 0);
}

} // namespace rd
