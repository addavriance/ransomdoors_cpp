#include "CoinManager.hpp"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <random>
#include <sstream>

namespace rd {

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

std::filesystem::path CoinManager::PickScatterDir() const {
    static const std::vector<const char*> candidates = {
        "Desktop", "Documents", "Downloads", "Pictures", "Music", "Videos",
    };
    std::filesystem::path home = HomeDir();

    static std::mt19937 rng{std::random_device{}()};
    std::vector<std::filesystem::path> existing = {home};
    for (const char* c : candidates) {
        std::filesystem::path dir = home / c;
        if (std::filesystem::exists(dir)) existing.push_back(dir);
    }

    std::uniform_int_distribution<std::size_t> dist(0, existing.size() - 1);
    std::filesystem::path base = existing[dist(rng)];

    // one level inside base folder
    std::vector<std::filesystem::path> subdirs;
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(
             base, std::filesystem::directory_options::skip_permission_denied, ec)) {
        if (ec) break;
        bool isDir = entry.is_directory(ec);
        if (!ec && isDir) subdirs.push_back(entry.path());
    }
    if (subdirs.empty()) return base;

    std::uniform_int_distribution<std::size_t> subDist(0, subdirs.size() - 1);
    return subdirs[subDist(rng)];
}

void CoinManager::ScatterRandomCoins(int count) {
    std::vector<std::filesystem::path> newPaths;
    for (int i = 0; i < count; ++i) {
        std::string token = RandomHex(8);
        std::string tag = Tag(token);

        std::filesystem::path dir = PickScatterDir();
        std::filesystem::path file = dir / ("coin_" + token.substr(0, 6) + ".gold");

        std::ofstream out(file, std::ios::trunc);
        if (!out) continue;
        out << token << "\n" << tag << "\n";
        out.close();

        activeCoinPaths_.push_back(file);
        newPaths.push_back(file);
    }
    if (!newPaths.empty()) AppendManifest(newPaths);
}

bool CoinManager::TryRedeem(const std::filesystem::path& path) {
    if (path.extension() != ".gold") return false;

    std::ifstream in(path);
    std::string token, tag;
    if (!in || !(in >> token) || !(in >> tag)) return false;
    in.close();

    if (tag != Tag(token)) return false;
    if (std::find(usedTokens_.begin(), usedTokens_.end(), token) != usedTokens_.end()) return false;

    usedTokens_.push_back(token);
    std::error_code ec;
    std::filesystem::remove(path, ec);
    RemoveFromManifest(path);

    auto it = std::find(activeCoinPaths_.begin(), activeCoinPaths_.end(), path);
    if (it != activeCoinPaths_.end()) activeCoinPaths_.erase(it);

    return true;
}

void CoinManager::DeleteAllCoins() {
    std::error_code ec;
    for (const auto& path : activeCoinPaths_) std::filesystem::remove(path, ec);
    activeCoinPaths_.clear();
    std::filesystem::remove(ManifestPath(configDir_), ec);
}

} // namespace rd
