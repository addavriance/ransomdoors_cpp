#include "CoinManager.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <random>
#include <sstream>

namespace rd {

namespace CoinValues {
int GetWeightedExtension() {
    static std::mt19937 rng{std::random_device{}()};
    int roll = static_cast<int>(rng() % 100);
    if (roll < 40) return 1;
    if (roll < 70) return 2;
    if (roll < 85) return 3;
    if (roll < 93) return 4;
    return 5;
}
} // namespace CoinValues

namespace {

std::string RandomHex(std::size_t bytes) {
    static std::mt19937_64 rng{std::random_device{}()};
    std::uniform_int_distribution<int> dist(0, 255);
    std::ostringstream out;
    for (std::size_t i = 0; i < bytes; ++i) {
        out << std::hex << std::setw(2) << std::setfill('0') << dist(rng);
    }
    return out.str();
}

std::filesystem::path HomeDir() {
#if defined(_WIN32)
    if (const char* p = std::getenv("USERPROFILE")) return std::filesystem::path(p);
#else
    if (const char* p = std::getenv("HOME")) return std::filesystem::path(p);
#endif
    return std::filesystem::temp_directory_path();
}

std::filesystem::path ManifestPath(const std::filesystem::path& configDir) {
    return configDir / "scattered_coins.manifest";
}

std::filesystem::path DrawerRoot() {
    return std::filesystem::temp_directory_path() / "RansomDrawers";
}

std::string RandomFolderName() { return RandomHex(4); } // 8 hex chars

std::vector<std::filesystem::path> ReadManifest(const std::filesystem::path& path) {
    std::vector<std::filesystem::path> out;
    std::ifstream in(path);
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty()) out.emplace_back(line);
    }
    return out;
}

void WriteManifest(const std::filesystem::path& path, const std::vector<std::filesystem::path>& paths) {
    std::ofstream out(path, std::ios::trunc);
    for (const auto& p : paths) out << p.string() << "\n";
}

} // namespace

CoinManager::CoinManager(std::filesystem::path configDir) : configDir_(std::move(configDir)) {
    std::filesystem::create_directories(configDir_);
    installKey_ = InstallKey();
    CleanupOrphaned();
}

void CoinManager::CleanupOrphaned() {
    std::filesystem::path manifest = ManifestPath(configDir_);
    std::error_code ec;
    for (const auto& p : ReadManifest(manifest)) std::filesystem::remove(p, ec);
    std::filesystem::remove(manifest, ec);
    std::filesystem::remove_all(DrawerRoot(), ec);
}

void CoinManager::AppendManifest(const std::vector<std::filesystem::path>& paths) const {
    std::ofstream out(ManifestPath(configDir_), std::ios::app);
    for (const auto& p : paths) out << p.string() << "\n";
}

void CoinManager::RemoveFromManifest(const std::filesystem::path& path) const {
    std::filesystem::path manifest = ManifestPath(configDir_);
    auto entries = ReadManifest(manifest);
    entries.erase(std::remove(entries.begin(), entries.end(), path), entries.end());
    WriteManifest(manifest, entries);
}

std::string CoinManager::InstallKey() {
    std::filesystem::path keyFile = configDir_ / "install.key";
    std::ifstream in(keyFile);
    std::string key;
    if (in && (in >> key) && !key.empty()) return key;

    key = RandomHex(16);
    std::ofstream out(keyFile, std::ios::trunc);
    out << key;
    return key;
}

std::string CoinManager::Tag(std::string_view token) const {
    // good enough, not real HMAC
    std::hash<std::string> hasher;
    std::string salted = installKey_ + ":" + std::string(token);
    std::size_t h = hasher(salted);
    std::ostringstream out;
    out << std::hex << h;
    return out.str();
}

std::filesystem::path CoinManager::PickScatterDir(int maxSubfolderDepth) const {
    std::filesystem::path home = HomeDir();

    static std::mt19937 rng{std::random_device{}()};
    std::vector<std::filesystem::path> bases;
    for (const char* c : kScatterFolderNames) {
        std::filesystem::path dir = home / c;
        if (std::filesystem::exists(dir)) bases.push_back(dir);
    }
    if (bases.empty()) bases.push_back(home);

    std::uniform_int_distribution<std::size_t> baseDist(0, bases.size() - 1);
    std::filesystem::path root = bases[baseDist(rng)];


    std::vector<std::filesystem::path> chain = {root};
    int depth = maxSubfolderDepth > 0 ? static_cast<int>(rng() % (maxSubfolderDepth + 1)) : 0;
    std::filesystem::path current = root;
    for (int i = 0; i < depth; ++i) {
        std::vector<std::filesystem::path> subdirs;
        std::error_code ec;
        for (const auto& entry : std::filesystem::directory_iterator(
                 current, std::filesystem::directory_options::skip_permission_denied, ec)) {
            if (ec) break;
            bool isDir = entry.is_directory(ec);
            if (!ec && isDir) subdirs.push_back(entry.path());
        }
        if (subdirs.empty()) break;
        std::uniform_int_distribution<std::size_t> subDist(0, subdirs.size() - 1);
        current = subdirs[subDist(rng)];
        chain.push_back(current);
    }

    std::uniform_int_distribution<std::size_t> chainDist(0, chain.size() - 1);
    return chain[chainDist(rng)];
}

bool CoinManager::WriteCoinFile(const std::filesystem::path& dir, const std::string& extension,
                                 std::vector<std::filesystem::path>& createdPaths) {
    std::string token = RandomHex(16); // 32 hex chars
    std::string tag = Tag(token);
    std::filesystem::path file = dir / (token + extension);

    std::ofstream out(file, std::ios::trunc);
    if (!out) return false;
    out << token << "\n" << tag << "\n";
    out.close();

    createdPaths.push_back(file);
    return true;
}

int CoinManager::ScatterRandomCoins(int targetGold, int maxSubfolderDepth) {
    static std::mt19937 rng{std::random_device{}()};
    std::vector<std::filesystem::path> newPaths;
    int generatedGold = 0;

    constexpr int kMaxAttempts = 500;
    int attempts = 0;
    while (generatedGold < targetGold && attempts < kMaxAttempts) {
        ++attempts;
        int extension = CoinValues::GetWeightedExtension(); // 1-5 only
        if (WriteCoinFile(PickScatterDir(maxSubfolderDepth), ".gold" + std::to_string(extension), newPaths)) {
            generatedGold += CoinValues::kValues[extension];
        }
    }

    constexpr double kHoneypotChance = 0.30;
    constexpr double kCrucifixChance = 0.10;
    std::uniform_real_distribution<double> chance(0.0, 1.0);
    if (chance(rng) < kHoneypotChance) {
        WriteCoinFile(PickScatterDir(maxSubfolderDepth), ".gold6", newPaths);
    }
    if (chance(rng) < kCrucifixChance) {
        WriteCoinFile(PickScatterDir(maxSubfolderDepth), ".crucifix", newPaths);
    }

    activeCoinPaths_.insert(activeCoinPaths_.end(), newPaths.begin(), newPaths.end());
    if (!newPaths.empty()) AppendManifest(newPaths);
    return generatedGold;
}

std::filesystem::path CoinManager::DrawerFolderPath() const { return DrawerRoot(); }

int CoinManager::ScatterDrawerCoins(int targetGold, int infectionDurationSec) {
    static std::mt19937 rng{std::random_device{}()};
    std::vector<std::filesystem::path> newPaths;
    std::vector<std::filesystem::path> allDirs;
    int generatedGold = 0;

    std::error_code ec;
    std::filesystem::path root = DrawerRoot();
    std::filesystem::remove_all(root, ec);
    std::filesystem::create_directories(root, ec);

    constexpr double kAverageCoinValue = 60.0;
    int slotsForGold = static_cast<int>(std::ceil(targetGold / kAverageCoinValue * 1.5));
    int slotsForDuration = infectionDurationSec / 2;
    int totalSlots = std::max(slotsForGold, slotsForDuration);
    int itemsPerDrawer = std::clamp(static_cast<int>(std::round(std::sqrt(totalSlots))), 3, 8);
    int drawerCount = std::max(1, static_cast<int>(std::ceil(static_cast<double>(totalSlots) / itemsPerDrawer)));

    for (int d = 0; d < drawerCount && generatedGold < targetGold; ++d) {
        std::filesystem::path drawerPath = root / RandomFolderName();
        std::filesystem::create_directories(drawerPath, ec);
        allDirs.push_back(drawerPath);

        for (int i = 0; i < itemsPerDrawer && generatedGold < targetGold; ++i) {
            std::filesystem::path itemPath = drawerPath / RandomFolderName();
            std::filesystem::create_directories(itemPath, ec);

            std::vector<std::filesystem::path> levels = {drawerPath, itemPath};
            if (rng() % 100 < 50) {
                std::filesystem::path subPath = itemPath / RandomFolderName();
                std::filesystem::create_directories(subPath, ec);
                levels.push_back(subPath);
            }
            for (std::size_t k = 1; k < levels.size(); ++k) allDirs.push_back(levels[k]);

            std::uniform_int_distribution<std::size_t> levelDist(0, levels.size() - 1);
            std::filesystem::path coinDir = levels[levelDist(rng)];

            int extension = CoinValues::GetWeightedExtension();
            if (WriteCoinFile(coinDir, ".gold" + std::to_string(extension), newPaths)) {
                generatedGold += CoinValues::kValues[extension];
            }
        }
    }

    if (!allDirs.empty()) {
        constexpr double kHoneypotChance = 0.30;
        constexpr double kCrucifixChance = 0.10;
        std::uniform_real_distribution<double> chance(0.0, 1.0);
        std::uniform_int_distribution<std::size_t> dirDist(0, allDirs.size() - 1);
        if (chance(rng) < kHoneypotChance) WriteCoinFile(allDirs[dirDist(rng)], ".gold6", newPaths);
        if (chance(rng) < kCrucifixChance) WriteCoinFile(allDirs[dirDist(rng)], ".crucifix", newPaths);
    }

    activeCoinPaths_.insert(activeCoinPaths_.end(), newPaths.begin(), newPaths.end());
    if (!newPaths.empty()) AppendManifest(newPaths);
    return generatedGold;
}

RedeemResult CoinManager::TryRedeem(const std::filesystem::path& path) {
    std::string ext = path.extension().string();
    bool isCrucifix = (ext == ".crucifix");
    int value = 0;
    if (!isCrucifix) {
        if (ext.size() != 6 || ext.rfind(".gold", 0) != 0) return {};
        int n = ext[5] - '0';
        if (n < 1 || n > 6) return {};
        value = CoinValues::kValues[n];
    }

    std::ifstream in(path);
    std::string token, tag;
    if (!in || !(in >> token) || !(in >> tag)) return {};
    in.close();

    if (tag != Tag(token)) return {};
    if (std::find(usedTokens_.begin(), usedTokens_.end(), token) != usedTokens_.end()) return {};

    usedTokens_.push_back(token);
    std::error_code ec;
    std::filesystem::remove(path, ec);
    RemoveFromManifest(path);

    auto it = std::find(activeCoinPaths_.begin(), activeCoinPaths_.end(), path);
    if (it != activeCoinPaths_.end()) activeCoinPaths_.erase(it);

    return {true, value, isCrucifix};
}

void CoinManager::DeleteAllCoins() {
    std::error_code ec;
    for (const auto& path : activeCoinPaths_) std::filesystem::remove(path, ec);
    activeCoinPaths_.clear();
    std::filesystem::remove(ManifestPath(configDir_), ec);
    std::filesystem::remove_all(DrawerRoot(), ec);
}

} // namespace rd
