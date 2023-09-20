#pragma once

#include <fmt/format.h>
#include <fstream>
#include <set>

struct PackageSupervisor {
    struct Package {
        bool explicitMarked;
        std::set<std::string> environments;
    };
    std::map<std::string, Package> packages;

    void storeFile(std::filesystem::path path) {
        auto ofs = std::ofstream{path, std::ios::binary};
        fmt::print(ofs, "version: 1\n");
        fmt::print(ofs, "packages:\n");
        for (auto const& [name, package] : packages) {
            fmt::print(ofs, "  - name: {}\n", name);
            fmt::print(ofs, "    explicitMarked: {}\n", package.explicitMarked?"true":"false");
            fmt::print(ofs, "    environments:\n");
            for (auto e : package.environments) {
                fmt::print(ofs, "      - {}\n", e);
            }
        }
    }
    void loadFile(std::filesystem::path path) {
        // Peek first line
        auto ifs = std::ifstream{path, std::ios::binary};
        if (!ifs.good()) throw std::runtime_error{"trouble loading PackageIndex file: " + path.string()};

        auto line = std::string{};
        if (!std::getline(ifs, line)) throw std::runtime_error{"trouble loading PackageIndex, empty file? " + path.string()};
        if (line.starts_with("version: 1")) {
            loadFileV1(path);
        } else {
            throw error_fmt("unknown file format version: {}\n", line);
        }
    }

    void loadFileV1(std::filesystem::path path) {
        auto ifs = std::ifstream{path, std::ios::binary};
        if (!ifs.good()) throw std::runtime_error{"trouble loading PackageIndex file: " + path.string()};

        auto line = std::string{};
        auto lastPackages = std::string{};
        while (std::getline(ifs, line)) {
            if (line.starts_with("  - name: ")) {
                lastPackages = line.substr(10);
            } else if (line.starts_with("    explicitMarked: ")) {
                if (line.substr(20) == "true") {
                    packages[lastPackages].explicitMarked = true;
                } else {
                    packages[lastPackages].explicitMarked = false;
                }
            } else if (line.starts_with("      - ")) {
                packages[lastPackages].environments.insert(line.substr(8));
            }
        }
    }


};
