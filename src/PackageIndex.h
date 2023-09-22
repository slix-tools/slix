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
#include <yaml-cpp/yaml.h>

struct PackageIndex {
    struct Info {
        std::string version;
        std::string hash;
        std::string description;
        std::vector<std::string> dependencies;
    };
    std::unordered_map<std::string, std::vector<Info>> packages;

    void storeFile(std::filesystem::path path) {
        auto yaml = YAML::Node{};
        yaml["version"] = 2;
        for (auto const& [key, infos] : packages) {
            auto node = YAML::Node{};
            node["name"] = key;
            for (auto const& info : infos) {
                auto node2 = YAML::Node{};
                node2["version"]     = info.version;
                node2["hash"]        = info.hash;
                node2["description"] = info.description;
                for (auto const& i : info.dependencies) {
                    node2["dependencies"].push_back(i);
                }
                node["versions"].push_back(node2);
            }
            yaml["packages"].push_back(node);
        }
        auto ofs = std::ofstream{path, std::ios::binary};
        auto emitter = YAML::Emitter{};
        emitter << yaml;
        fmt::print(ofs, "{}", emitter.c_str());
    }
    void loadFile(std::filesystem::path path) {
        auto yaml = YAML::LoadFile(path);
        if (yaml["version"].as<int>() == 2) {
            loadFileV2(yaml);
        } else {
            throw error_fmt("unknown file format");
        }
    }
    void loadFileV2(YAML::Node yaml) {
        for (auto p : yaml["packages"]) {
            auto name = p["name"].as<std::string>();
            for (auto const& e : p["versions"]) {
                auto info = Info {
                    .version     = e["version"].as<std::string>(),
                    .hash        = e["hash"].as<std::string>(),
                    .description = e["description"].as<std::string>(),
                };
                for (auto d : e["dependencies"]) {
                    info.dependencies.push_back(d.as<std::string>());
                }
                packages[name].push_back(info);
            }
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
        // prefer exact hit, over approximate hit
        for (auto const& [path, index] : indices) {
            for (auto const& [key, infos] : index.packages) {
                if (key == name) {
                    auto const& info = infos.back();
                    auto res = std::make_tuple<std::filesystem::path, PackageIndex const*, std::string, PackageIndex::Info const*>(std::filesystem::path{path}, &index, std::string{key}, &info);
                    return res;
                }
            }
        }
        // search again, for approximate
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
    auto findMultiLatest(std::string_view name) -> std::vector<std::tuple<std::filesystem::path, PackageIndex const*, std::string, PackageIndex::Info const*>> {
        auto resList = std::vector<std::tuple<std::filesystem::path, PackageIndex const*, std::string, PackageIndex::Info const*>>{};
        // prefer exact hit, over approximate hit
        for (auto const& [path, index] : indices) {
            for (auto const& [key, infos] : index.packages) {
                if (key == name) {
                    auto const& info = infos.back();
                    auto res = std::make_tuple<std::filesystem::path, PackageIndex const*, std::string, PackageIndex::Info const*>(std::filesystem::path{path}, &index, std::string{key}, &info);
                    resList.push_back(res);
                }
            }
        }
        if (!resList.empty()) return resList;

        // search again, for approximate
        for (auto const& [path, index] : indices) {
            for (auto const& [key, infos] : index.packages) {
                if (key.starts_with(name) and key != name) {
                    auto const& info = infos.back();
                    auto res = std::make_tuple<std::filesystem::path, PackageIndex const*, std::string, PackageIndex::Info const*>(std::filesystem::path{path}, &index, std::string{key}, &info);
                    resList.push_back(res);
                }
            }
        }
        return resList;
    }

    auto findInstalled(std::string_view name, std::unordered_set<std::string> const& istPkgs) -> std::tuple<std::string, PackageIndex::Info> {
        for (auto const& [path, index] : indices) {
            for (auto const& [key, infos] : index.packages) {
                // search for newest version
                if (key == name) {
                    auto const& info = infos.back();
                    auto s = fmt::format("{}@{}#{}", key, info.version, info.hash);
                    if (istPkgs.contains(s + ".gar")) {
                         return {s, info};
                    }
                }
                // search for exact version
                for (auto const& info : infos) {
                    auto s = fmt::format("{}@{}#{}", key, info.version, info.hash);
                    if (s == name) {
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
