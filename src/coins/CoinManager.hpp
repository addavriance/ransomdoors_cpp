#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace rd {

// Not real DPAPI, portable HMAC-ish tag instead.
class CoinManager {
public:
    explicit CoinManager(std::filesystem::path configDir);

    void ScatterRandomCoins(int count);
    void DeleteAllCoins();

    bool TryRedeem(const std::filesystem::path& path);

    int RemainingCoins() const { return static_cast<int>(activeCoinPaths_.size()); }

private:
    std::string InstallKey();
    std::string Tag(std::string_view token) const;
    std::filesystem::path PickScatterDir() const;

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
