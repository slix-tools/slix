#pragma once
#include "PackageIndex.h"

#include <cstdlib>
#include <filesystem>
#include <random>
#include <ranges>
#include <unordered_set>
#include <string>
#include <string_view>

inline auto create_temp_dir() -> std::filesystem::path {
    auto tmp_dir = std::filesystem::temp_directory_path();
    auto rdev = std::random_device{};
    auto prng = std::mt19937{rdev()};
    auto rand = std::uniform_int_distribution<uint64_t>{0};
    for (size_t i{0}; i < 100; ++i) {
        auto ss = std::stringstream{};
        ss << "slix-fs-" << std::hex << rand(prng);
        auto path = tmp_dir / ss.str();
        // true if the directory was created.
        if (std::filesystem::create_directory(path)) return path;
    }
    throw std::runtime_error{"failed creating temporary directory"};
}

/**
 * Path to the ~/.config/slix path
 */
inline auto getSlixConfigPath() -> std::filesystem::path {
    auto ptr = std::getenv("XDG_CONFIG_HOME");
    if (ptr) return ptr + std::string{"/slix"};
    ptr = std::getenv("HOME");
    if (ptr) return ptr + std::string{"/.config/slix"};
    throw std::runtime_error{"unknown HOME and XDG_CONFIG_HOME"};
}

/**
 * Path to the ~/.local/state/slix path
 */
inline auto getSlixStatePath() -> std::filesystem::path {
    auto ptr = std::getenv("XDG_STATE_HOME");
    if (ptr) return ptr + std::string{"/slix"};
    ptr = std::getenv("HOME");
    if (ptr) return ptr + std::string{"/.local/state/slix"};
    throw std::runtime_error{"unknown HOME and XDG_STATE_HOME"};
}


/**
 * get path to upstream directories
 */
inline auto getUpstreamsPath() -> std::filesystem::path {
    auto path = getSlixConfigPath() / "upstreams";
    if (!exists(path)) {
        throw std::runtime_error{"missing path: " + path.string()};
    }
    return path;
}

/** returns a list of installed packages - including the trailing .gar
 */
inline auto installedPackages(std::vector<std::filesystem::path> const& slixPkgPaths) -> std::unordered_set<std::string> {
    auto results = std::unordered_set<std::string>{};
    for (auto p : slixPkgPaths) {
        for (auto pkg : std::filesystem::directory_iterator{p}) {
            results.insert(pkg.path().filename().string());
        }
    }
    return results;
}

/** searches through paths for a package with exact name
 */
inline auto searchPackagePath(std::vector<std::filesystem::path> const& slixPkgPaths, std::string const& name) -> std::filesystem::path {
    for (auto p : slixPkgPaths) {
        for (auto pkg : std::filesystem::directory_iterator{p}) {
            auto filename = pkg.path().filename();
            if (filename == name) {
                return pkg.path();
            }
        }
    }
    throw std::runtime_error{"couldn't find path for " + name};
}

/**
 * load all package indices
 */
inline auto loadPackageIndices() -> PackageIndices {
    auto pathUpstreams = getUpstreamsPath();
    auto res = PackageIndices{};
    for (auto const& e : std::filesystem::directory_iterator{pathUpstreams}) {
        if (e.path().extension() != ".db") continue;
        auto& index = res.indices[e.path()];
        index.loadFile(e.path());
    }
    return res;
}
