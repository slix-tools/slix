#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <filesystem>
#include <fstream>

struct PackageIndex {
    struct Info {
        std::vector<std::string> dependencies;
    };
    std::unordered_map<std::string, Info> packages;

    void storeFile(std::filesystem::path path) {
        auto ofs = std::ofstream{path, std::ios::binary};
        for (auto const& [key, info] : packages) {
            ofs << key << ":\n";
            for (auto const& i : info.dependencies) {
                ofs << "  - " << i << "\n";
            }
        }
    }
    void loadFile(std::filesystem::path path) {
        auto entry = std::optional<std::pair<std::string, Info>>{};

        auto ifs = std::ifstream{path, std::ios::binary};
        auto line = std::string{};
        while (std::getline(ifs, line)) {
            if (line.empty()) continue;
            if (line.starts_with("  - ")) {
                if (!entry) throw std::runtime_error{"unexpected entry in " + path.string()};
                entry->second.dependencies.push_back(line.substr(4));
            } else {
                if (entry) {
                    packages.try_emplace(entry->first, entry->second);
                    entry.reset();
                }
                line = line.substr(0, line.size()-1);
                entry.emplace(line, Info{});
            }
        }
        if (entry) {
            packages.try_emplace(entry->first, entry->second);
            entry.reset();
        }
    }
};
