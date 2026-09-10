#include "Config.hpp"

#include <fstream>

#include <nlohmann/json.hpp>

namespace rd {

namespace {
std::filesystem::path ConfigFile(const std::filesystem::path& configDir) {
    return configDir / "config.json";
}
} // namespace

void Config::Load(const std::filesystem::path& configDir) {
    std::ifstream in(ConfigFile(configDir));
    if (!in) return; // no file yet - keep defaults

    nlohmann::json j;
    try {
        in >> j;
    } catch (const nlohmann::json::parse_error&) {
        return; // malformed file - keep defaults rather than crash
    }

    spawnAutomatically = j.value("spawnAutomatically", spawnAutomatically);
    minSpawnDelaySec = j.value("minSpawnDelaySec", minSpawnDelaySec);
    maxSpawnDelaySec = j.value("maxSpawnDelaySec", maxSpawnDelaySec);
    infectionDurationSec = j.value("infectionDurationSec", infectionDurationSec);
    ransomAmount = j.value("ransomAmount", ransomAmount);
    useDrawerMode = j.value("useDrawerMode", useDrawerMode);
    cmdOnDeath = j.value("cmdOnDeath", cmdOnDeath);
    // execCmdOnDeath is intentionally never read from disk - see Config.hpp.
}

void Config::Save(const std::filesystem::path& configDir) const {
    std::error_code ec;
    std::filesystem::create_directories(configDir, ec);

    nlohmann::json j{
        {"spawnAutomatically", spawnAutomatically},
        {"minSpawnDelaySec", minSpawnDelaySec},
        {"maxSpawnDelaySec", maxSpawnDelaySec},
        {"infectionDurationSec", infectionDurationSec},
        {"ransomAmount", ransomAmount},
        {"useDrawerMode", useDrawerMode},
        {"cmdOnDeath", cmdOnDeath},
        // execCmdOnDeath intentionally never written - see Config.hpp.
    };

    std::ofstream out(ConfigFile(configDir), std::ios::trunc);
    if (out) out << j.dump(2) << "\n";
}

} // namespace rd
