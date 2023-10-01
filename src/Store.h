#pragma once

#include "error_fmt.h"

#include <filesystem>
#include <fmt/format.h>
#include <fstream>
#include <string>
#include <vector>
#include <yaml-cpp/yaml.h>

struct StoreConfig {
    std::string           type;
    struct Source {
        std::string           type;
        std::filesystem::path url;
    } source;

    static auto defaultStoreConfig() {
        return StoreConfig {
            .type = "local",
            .source = {
                StoreConfig::Source {
                    .type = "https",
                    .url  = "slix-tools.de/packages/",
                }
            },
        };
    }


    /** \brief Save this store config into a file.
     *!TODO must be changed to an atomic write
     */
    void save(std::filesystem::path _path) {
        std::filesystem::create_directories(_path.parent_path());

        auto yaml = YAML::Node{};
        yaml["version"] = 1;
        yaml["type"]    = type;
        yaml["source"]["type"] = source.type;
        yaml["source"]["url"]  = source.url.string();

        auto ofs = std::ofstream{_path, std::ios::binary};
        auto emitter = YAML::Emitter{};
        emitter << yaml;
        fmt::print(ofs, "{}", emitter.c_str());
    }


    /** \brief load store config from file
     */
    void load(std::filesystem::path path) {
        auto yaml = YAML::LoadFile(path);
        if (yaml["version"].as<int>() == 1) {
            loadV1(yaml);
        } else {
            throw error_fmt("unknown file format");
        }
    }
private:
    void loadV1(YAML::Node yaml) {
        type   = yaml["type"].as<std::string>();
        source.type = yaml["source"]["type"].as<std::string>();
        source.url  = yaml["source"]["url"].as<std::string>();
    }
};

/**
 * Stores the state of this store.
 *
 * The member `packages` map of all packages that are installed.
 * Each packages is identified by its name and map to a list of installed canonical names.
 */
struct StoreState {
    std::unordered_map<std::string, std::unordered_set<std::string>> packages;

    /** Check if full qualified name is installed
     */
    bool isInstalled(std::string pattern) const {
        auto posAt   = pattern.rfind('@');
        auto posHash = pattern.rfind('#');
        if (posAt == std::string::npos || posHash == std::string::npos) return false;
        auto name    = pattern.substr(0, posAt);
        auto version = pattern.substr(posAt+1, posHash-posAt-1);
        auto hash    = pattern.substr(posHash+1);
        auto iter = packages.find(name);
        if (iter == packages.end()) return false;

        return iter->second.contains(pattern);
    }

    /** \brief Save this store config into a file.
     *!TODO must be changed to an atomic write
     */
    void save(std::filesystem::path _path) {
        std::filesystem::create_directories(_path.parent_path());

        auto yaml = YAML::Node{};
        yaml["version"] = 1;
        for (auto const& [name, list] : packages) {
            for (auto e : list) {
                yaml["packages"][name].push_back(e);
            }
        }

        auto ofs = std::ofstream{_path, std::ios::binary};
        auto emitter = YAML::Emitter{};
        emitter << yaml;
        fmt::print(ofs, "{}", emitter.c_str());
    }


    /** \brief load store config from file
     */
    void load(std::filesystem::path path) {
        auto yaml = YAML::LoadFile(path);
        if (yaml["version"].as<int>() == 1) {
            loadV1(yaml);
        } else {
            throw error_fmt("unknown file format");
        }
    }
private:
    void loadV1(YAML::Node yaml) {
        for (auto const& pair : yaml["packages"]) {
            auto& package = packages[pair.first.as<std::string>()];
            for (auto e : pair.second) {
                package.insert(e.as<std::string>());
            }
        }
    }
};

struct Store {
    std::string name;
    StoreConfig config;
    StoreState  state;

    Store() = default;
    Store(Store const&) = default;
    Store(Store&&) = default;
    Store(std::filesystem::path path) {
        name = path.stem();
        config.load(path);
        state.load(getSlixStatePath() / name / "state.yaml");
    }

public:
    /** \brief download stores newest file
     */
    void update() {
        auto const& s = config.source;
        if (s.type == "https") {
            fmt::print("  - https://{} ({})\n", s.url, s.type);

            auto source = fmt::format("https://{}", s.url / "index.db");
            auto dest   = getSlixCachePath() / "stores" / name / fmt::format("index.db");
            std::filesystem::create_directories(std::filesystem::path{dest}.parent_path());
            downloadFile(source, dest, false);

        } else {
            throw error_fmt{"unknown source type {}", s.type};
        }
    }

    auto loadPackageIndex() const {
        auto dest   = getSlixCachePath() / "stores" / name / fmt::format("index.db");
        return PackageIndex{dest};
    }

    auto getPackagePath(std::string fullPackageName) const -> std::filesystem::path {
        return getSlixStatePath() / this->name / "packages" / (fullPackageName + ".gar");
    }

    bool isInstalled(std::string fullPackageName) const {
        return state.isInstalled(fullPackageName);
    }

    //!TODO not atomic writing
    void install(std::string const& pattern, bool verbose) {
        auto posAt   = pattern.rfind('@');
        auto posHash = pattern.rfind('#');
        if (posAt   == std::string::npos || posHash == std::string::npos) throw error_fmt{"invalid package name format {}", pattern};
        auto name    = pattern.substr(0, posAt);
        auto version = pattern.substr(posAt+1, posHash-posAt-1);
        auto hash    = pattern.substr(posHash+1);


        StoreConfig::Source const& source = config.source;
        if (config.type == "local") {

            auto package = pattern + ".gar.zst";
            auto src  = "https://" + (source.url / package).string();
            auto dest = getSlixStatePath() / this->name / "packages" / package;

            std::filesystem::create_directories(dest.parent_path());

            if (source.type == "https") {
                downloadFile(encodeURL(src), dest, verbose);
            } else {
                throw error_fmt{"unknown source type {}", source.type};
            }
            unpackZstFile(dest);

            state.packages[name].insert(pattern);
        } else {
            throw error_fmt{"unknown store type {}", config.type};
        }
    }
};
