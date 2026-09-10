#pragma once

#include <cstdint>
#include <filesystem>

#include <string>

namespace rd {

struct Config {
    bool spawnAutomatically = true;
    int minSpawnDelaySec = 5;
    int maxSpawnDelaySec = 600;
    int infectionDurationSec = 90;
    int ransomAmount = 500;
    bool useDrawerMode = false;

    // both session-only: never read from or written to disk, always reset false on launch
    bool crashOnDeath = false;
    bool execCmdOnDeath = false;
    std::string cmdOnDeath = "shutdown /s /t 0";

    void Load(const std::filesystem::path& configDir);
    void Save(const std::filesystem::path& configDir) const;
};

} // namespace rd
