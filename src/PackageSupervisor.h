#pragma once

#include <fmt/format.h>
#include <fstream>
#include <set>
#include <yaml-cpp/yaml.h>

struct PackageSupervisor {
    struct Package {
        bool explicitMarked;
        std::set<std::string> environments;
    };
    std::map<std::string, Package> packages;

    void storeFile(std::filesystem::path path) {
        auto yaml = YAML::Node{};
        yaml["version"] = 1;
        for (auto const& [name, package] : packages) {
            auto yamlPackages = YAML::Node{};
            yamlPackages["name"] = name;
            yamlPackages["explicitMarked"] = package.explicitMarked;
            for (auto e : package.environments) {
                yamlPackages["environments"].push_back(e);
            }
            yaml["packages"].push_back(yamlPackages);
        }
        auto ofs = std::ofstream{path, std::ios::binary};
        auto emitter = YAML::Emitter{};
        emitter << yaml;
        fmt::print(ofs, "{}", emitter.c_str());

    }
    void loadFile(std::filesystem::path path) {
        auto yaml = YAML::LoadFile(path);
        if (yaml["version"].as<int>() == 1) {
            loadFileV1(yaml);
        } else {
            throw error_fmt("unknown file format");
        }
    }

    void loadFileV1(YAML::Node yaml) {
        for (auto p : yaml["packages"]) {
            auto name                     = p["name"].as<std::string>();
            packages[name].explicitMarked = p["explicitMarked"].as<bool>();
            for (auto e : p["environments"]) {
                packages[name].environments.insert(e.as<std::string>());
            }
        }
    }


};
