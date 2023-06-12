#include "slix.h"
#include "utils.h"
#include "PackageIndex.h"
#include "UpstreamConfig.h"

#include <clice/clice.h>
#include <stdexcept>
#include <fstream>
#include <iostream>
#include <map>
#include <set>

namespace {
void app();
auto cli = clice::Argument{ .arg    = "sync",
                            .desc   = "synchronizes a package and its dependencies",
                            .value  = std::vector<std::string>{},
                            .cb     = app,
};

auto installedPackages(std::vector<std::filesystem::path> const& slixPkgPaths) -> std::unordered_set<std::string> {
    auto results = std::unordered_set<std::string>{};
    for (auto p : slixPkgPaths) {
        for (auto pkg : std::filesystem::directory_iterator{p}) {
            results.insert(pkg.path().filename().string());
        }
    }
    return results;
}

void app() {
    auto path = getSlixConfigPath() / "upstreams";
    if (!exists(path)) {
        throw std::runtime_error{"missing path: " + path.string()};
    }


    auto slixPkgPaths = getSlixPkgPaths();
    auto istPkgs      = installedPackages(slixPkgPaths);

    auto reqPkgs = std::unordered_set<std::string>{};
    auto openPackages = std::set<std::string>{};

    // load all indices
    auto indices = std::map<std::string, PackageIndex>{};
    for (auto const& e : std::filesystem::directory_iterator{path}) {
        if (e.path().extension() != ".db") continue;
        auto& index = indices[e.path()];
        index.loadFile(e.path());
    }


    for (auto p : *cli) {
        p = [&]() {
            auto closeHits = std::set<std::string>{};
            for (auto const& e : std::filesystem::directory_iterator{path}) {
                if (e.path().extension() != ".db") continue;
                auto& index = indices[e.path()];
                for (auto const& [key, info] : index.packages) {
                    if (key == p) return key;
                    if (key.starts_with(p + "@")) {
                        closeHits.insert(key);
                    }
                }
            }
            if (closeHits.size() == 1) {
                return *closeHits.begin();
            }
            if (closeHits.empty()) throw std::runtime_error{"Could not find " + p};
            auto msg = "Ambigous: multiple results for " + p + "\n";
            for (auto h : closeHits) {
                msg += h + "\n";
            }
            throw std::runtime_error{msg};
        }();

        reqPkgs.insert(p);
        openPackages.insert(p);
    }

    // also add all dependencies
    for (auto const& e : std::filesystem::directory_iterator{path}) {
        if (e.path().extension() != ".db") continue;
        auto& index = indices[e.path()];
        for (auto const& [key, info] : index.packages) {
            if (openPackages.contains(key)) {
                openPackages.erase(key);
                for (auto const& d : info.dependencies) {
                    reqPkgs.insert(d);
                }
            }
        }
    }
    if (!openPackages.empty()) {
        std::cout << "could not find packages:\n";
        for (auto p : openPackages) {
            std::cout << "  - " << p << "\n";
               exit(1);
        }
    }

    // install all packages
    for (auto p : reqPkgs) {
        if (istPkgs.contains(p)) continue; // already installed
        [&]() {
            for (auto const& [path, index] : indices) {
                if (index.packages.contains(p)) {
                    auto config_path = std::filesystem::path{path};
                    config_path.replace_extension();
                    auto config = UpstreamConfig{};
                    config.loadFile(config_path);

                    if (!config.valid()) throw std::runtime_error{"invalid config " + config_path.string()};
                    if (config.type == "ignore") continue;

                    std::cout << "found " << p << " at " << path << "\n";
                    if (config.type == "file") {
                        auto source = std::filesystem::path{config.path} / p;
                        auto dest   = getSlixConfigPath() / "packages" / p;
                        std::filesystem::copy_file(source, dest, std::filesystem::copy_options::overwrite_existing);
                    } else if (config.type == "https") {
                        p += ".zst";
                        auto source = std::filesystem::path{config.path} / p;
                        auto dest   = getSlixConfigPath() / "packages" / p;
                        {
                            auto call = "curl \"" + source.string() + "\" -o \"" + dest.string() + "\"";
                            std::system(call.c_str());
                        }
                        {
                            auto call = "zstd -d --rm " + dest.string();
                            std::system(call.c_str());
                        }
                    } else {
                        throw std::runtime_error{"unknown upstream type " + config.type};
                    }
                    return;
                }
            }
        }();
    }
}
}
