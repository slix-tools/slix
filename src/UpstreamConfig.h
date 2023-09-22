#pragma once

#include "error_fmt.h"

#include <filesystem>
#include <fmt/format.h>
#include <fstream>
#include <string>
#include <vector>
#include <yaml-cpp/yaml.h>

struct UpstreamConfig {
    std::string path = "https://slix-tools.de/packages";
    std::string type = "https";
    bool shared = false;

    void storeFile(std::filesystem::path path) {
        std::filesystem::create_directories(path.parent_path());

        auto yaml = YAML::Node{};
        yaml["version"] = 1;
        yaml["path"]    = path.string();
        yaml["type"]    = type;
        yaml["shared"]  = shared;

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
        path   = yaml["path"].as<std::string>();
        type   = yaml["type"].as<std::string>();
        shared = yaml["shared"].as<bool>();
    }
};
