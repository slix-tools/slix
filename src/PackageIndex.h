#pragma once

#include "UpstreamConfig.h"
#include "error_fmt.h"

#include <filesystem>
#include <fmt/format.h>
#include <fmt/std.h>
#include <fstream>
#include <optional>
#include <string>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <vector>

struct PackageIndex {
    struct Info {
        std::string version;
        std::string hash;
        std::vector<std::string> dependencies;
    };
    std::unordered_map<std::string, std::vector<Info>> packages;

    void storeFile(std::filesystem::path path) {
        auto ofs = std::ofstream{path, std::ios::binary};
        for (auto const& [key, infos] : packages) {
            ofs << key << ":\n";
            for (auto const& info : infos) {
                ofs << "  - " << info.hash << " " << info.version << "\n";
                for (auto const& i : info.dependencies) {
                    ofs << "    - " << i << "\n";
                }
            }
        }
    }
    void loadFile(std::filesystem::path path) {
        auto entry = std::optional<std::pair<std::string, std::vector<Info>>>{};

        auto ifs = std::ifstream{path, std::ios::binary};
        auto line = std::string{};
        if (!ifs.good()) throw std::runtime_error{"trouble loading PackageIndex file: " + path.string()};
        while (std::getline(ifs, line)) {
            if (line.empty()) continue;
            if (line.starts_with("  - ")) {
                if (!entry) throw std::runtime_error{"unexpected entry in " + path.string()};
                line = line.substr(4);
                auto pos = line.find(' ');
                if (pos == std::string::npos) throw std::runtime_error{"unexpected entry in " + path.string()};
                entry->second.push_back(Info{});
                entry->second.back().version = line.substr(pos+1);
                entry->second.back().hash    = line.substr(0, pos);
            } else if (line.starts_with("    - ")) {
                if (!entry) throw std::runtime_error{"unexpected entry in " + path.string()};
                entry->second.back().dependencies.push_back(line.substr(6));
            } else {
                if (entry and !entry->second.empty()) {
                    packages.try_emplace(entry->first, entry->second);
                    entry.reset();
                }
                line = line.substr(0, line.size()-1);
                entry.emplace(line, std::vector<Info>{});
            }
        }
        if (entry) {
            packages.try_emplace(entry->first, entry->second);
            entry.reset();
        }
    }
};

struct PackageIndices {
    std::unordered_map<std::filesystem::path, PackageIndex>                                indices;
    std::map<std::string, std::tuple<std::string, std::string, PackageIndex::Info const*>> pkgToInfo;

    PackageIndices(std::unordered_map<std::filesystem::path, PackageIndex> _indices)
        : indices{std::move(_indices)}
    {
        for (auto const& [path, index] : indices) {
           for (auto const& [key, infos] : index.packages) {
                for (auto const& info : infos) {
                    auto s = fmt::format("{}@{}#{}", key, info.version, info.hash);
                    pkgToInfo[s] = {path.string(), key, &info};
                }
            }
        }
    }
    PackageIndices(PackageIndices const&) = delete;
    PackageIndices(PackageIndices&&) = default;

    auto findLatest(std::string_view name) -> std::optional<std::tuple<std::filesystem::path, PackageIndex const*, std::string, PackageIndex::Info const*>> {
        for (auto const& [path, index] : indices) {
            for (auto const& [key, infos] : index.packages) {
                if (key.starts_with(name)) {
                    auto const& info = infos.back();
                    auto res = std::make_tuple<std::filesystem::path, PackageIndex const*, std::string, PackageIndex::Info const*>(std::filesystem::path{path}, &index, std::string{key}, &info);
                    return res;
                }
            }
        }
        return std::nullopt;
    }
    auto findInstalled(std::string_view name, std::unordered_set<std::string> const& istPkgs) -> std::tuple<std::string, PackageIndex::Info> {
        for (auto const& [path, index] : indices) {
            for (auto const& [key, infos] : index.packages) {
                for (auto const& info : infos) {
                    auto s = fmt::format("{}@{}#{}", key, info.version, info.hash);
                    if (key == name or s == name) {
                        if (istPkgs.contains(s + ".gar")) {
                            return {s, info};
                        }
                    }
                }
            }
        }
        return {};
    }

    /** find package
     */
    auto findPackageInfo(std::string_view pattern) -> std::tuple<std::string, PackageIndex::Info const*> {
        for (auto const& [path, index] : indices) {
           for (auto const& [key, infos] : index.packages) {
                for (auto const& info : infos) {
                    auto s = fmt::format("{}@{}#{}", key, info.version, info.hash);
                    if (s == pattern) return {s, &info};
                }
                if (key == pattern) {
                    auto const& info = infos.back();
                    auto s = fmt::format("{}@{}#{}", key, info.version, info.hash);
                    return {s, &info};
                }
            }
        }
        throw error_fmt{"Could not find any entry for {}", pattern};
    }
    /** find required/dependency packages
     */
    auto findDependencies(std::string_view pattern) -> std::unordered_set<std::string> {
        auto res = std::unordered_set<std::string>{};
        auto [key, info] = findPackageInfo(pattern);
        res.insert(key);
        for (auto const& d : info->dependencies) {
            res.insert(d);
        }
        return res;
    }

    /** find fitting UpstreamConfig for a certain package
     */
    auto findPackageConfig(std::string package) -> UpstreamConfig {
        // check if package is available on any remote
        auto iter = pkgToInfo.find(package);
        if (iter == pkgToInfo.end()) throw error_fmt{"missing upstream source for {}", package};

        auto const& [path, key, info] = iter->second;

        // Load Upstream Config
        auto config_path = std::filesystem::path{path};
        config_path.replace_extension();
        auto config = UpstreamConfig{};
        config.loadFile(config_path);

        if (!config.valid()) throw error_fmt{"invalid config {}", config_path.string()};
        return config;
    }
};
