#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <filesystem>
#include <fstream>

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
                if (entry) {
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
