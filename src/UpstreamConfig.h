#pragma once

#include <filesystem>
#include <string>
#include <fstream>
#include <vector>

struct UpstreamConfig {
    std::string path;
    std::string type;

    static auto readLines(std::filesystem::path const& script) -> std::vector<std::string> {
        auto results = std::vector<std::string>{};
        auto ifs = std::ifstream{script};
        auto line = std::string{};
        while (std::getline(ifs, line)) {
            results.push_back(line);
        }
        return results;
    }

    void loadFile(std::filesystem::path _path) {
        auto lines = readLines(_path);
        for (auto l : lines) {
            if (l.starts_with("path=")) path = l.substr(5);
            if (l.starts_with("type=")) type = l.substr(5);
        }
    }
    bool valid() const {
        return !path.empty() and !type.empty();
    }
};
