#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace rd {

namespace CoinValues {
constexpr int kValues[7] = {0, 10, 50, 100, 150, 200, 500}; // index = extension
int GetWeightedExtension();
} // namespace CoinValues

// folders PickScatterDir() picks from (relative to $HOME/%USERPROFILE%); shared with
// Platform::RequestPermissions()
constexpr const char* kScatterFolderNames[] = {
    "Desktop", "Documents", "Downloads", "Pictures", "Music", "Videos",
};

struct RedeemResult {
    bool ok = false;
    int value = 0;
    bool isCrucifix = false;
};

// Not real DPAPI, portable HMAC-ish tag instead.
class CoinManager {
public:
    explicit CoinManager(std::filesystem::path configDir);


    int ScatterRandomCoins(int targetGold, int maxSubfolderDepth);
    int ScatterDrawerCoins(int targetGold, int infectionDurationSec);
    std::filesystem::path DrawerFolderPath() const;
    void DeleteAllCoins();

    RedeemResult TryRedeem(const std::filesystem::path& path);

    int RemainingCoins() const { return static_cast<int>(activeCoinPaths_.size()); }

private:
    std::string InstallKey();
    std::string Tag(std::string_view token) const;
    std::filesystem::path PickScatterDir(int maxSubfolderDepth) const;
    bool WriteCoinFile(const std::filesystem::path& dir, const std::string& extension,
                        std::vector<std::filesystem::path>& createdPaths);

    // survives crash/kill via manifest file
    void CleanupOrphaned();
    void AppendManifest(const std::vector<std::filesystem::path>& paths) const;
    void RemoveFromManifest(const std::filesystem::path& path) const;

    std::filesystem::path configDir_;
    std::string installKey_;
    std::vector<std::filesystem::path> activeCoinPaths_;
    std::vector<std::string> usedTokens_;
};

} // namespace rd
