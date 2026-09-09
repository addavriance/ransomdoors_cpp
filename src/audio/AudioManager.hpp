#pragma once

#include <SDL_mixer.h>

#include <string>
#include <string_view>
#include <unordered_map>

namespace rd {

class AudioManager {
public:
    bool Init();
    void Shutdown();

    void LoadMusic(std::string_view key, const std::string& path);
    void LoadSfx(std::string_view key, const std::string& path);

    void PlayMusicLoop(std::string_view key);
    void StopMusic();
    void PlaySfx(std::string_view key, float volume = 1.0f);

private:
    std::unordered_map<std::string, Mix_Music*> music_;
    std::unordered_map<std::string, Mix_Chunk*> sfx_;
};

} // namespace rd
