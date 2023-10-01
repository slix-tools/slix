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

     PackageIndex() = default;
     PackageIndex(std::filesystem::path path) {
        loadFile(path);
     }

    void storeFile(std::filesystem::path path) {
        std::filesystem::create_directories(path.parent_path());

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

    /** find package
     */
    auto findPackageInfo(std::string_view pattern) -> std::tuple<std::string, Info const*> {
       for (auto const& [key, infos] : packages) {
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
};
