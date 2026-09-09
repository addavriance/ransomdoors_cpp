#pragma once

#include <filesystem>
#include <string>

namespace rd {

class HardModeConsent {
public:
    explicit HardModeConsent(std::filesystem::path configDir);

    bool IsEnabled() const;
    void Enable();
    void Disable();

private:
    std::filesystem::path ConfigFile() const;

    std::filesystem::path configDir_;
};

} // namespace rd
