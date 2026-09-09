#include "HardModeConsent.hpp"

#include <fstream>

namespace rd {

HardModeConsent::HardModeConsent(std::filesystem::path configDir) : configDir_(std::move(configDir)) {
    std::filesystem::create_directories(configDir_);
}

std::filesystem::path HardModeConsent::ConfigFile() const {
    return configDir_ / "hardmode.consent";
}

bool HardModeConsent::IsEnabled() const {
    std::ifstream in(ConfigFile());
    if (!in) return false;
    std::string line;
    return static_cast<bool>(std::getline(in, line)) && line == "enabled";
}

void HardModeConsent::Enable() {
    std::ofstream out(ConfigFile(), std::ios::trunc);
    if (out) out << "enabled\n";
}

void HardModeConsent::Disable() {
    std::error_code ec;
    std::filesystem::remove(ConfigFile(), ec);
}

} // namespace rd
