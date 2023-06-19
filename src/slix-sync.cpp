#include "slix.h"
#include "utils.h"
#include "PackageIndex.h"
#include "UpstreamConfig.h"

#include <clice/clice.h>
#include <stdexcept>
#include <fstream>
#include <iostream>
#include <fmt/format.h>
#include <fmt/std.h>
#include <map>
#include <set>

namespace {
void app();
auto cli = clice::Argument{ .arg    = "sync",
                            .desc   = "synchronizes a package and its dependencies",
                            .value  = std::vector<std::string>{},
                            .cb     = app,
};

void app() {
    auto path = getSlixConfigPath() / "upstreams";
    if (!exists(path)) {
        throw std::runtime_error{"missing path: " + path.string()};
    }


    auto slixPkgPaths = getSlixPkgPaths();
    auto istPkgs      = installedPackages(slixPkgPaths);

    // load all indices
    auto indices = std::map<std::string, PackageIndex>{};
    auto pkgToInfo = std::map<std::string, std::tuple<std::string, std::string, PackageIndex::Info const*>>{};
    for (auto const& e : std::filesystem::directory_iterator{path}) {
        if (e.path().extension() != ".db") continue;
        auto& index = indices[e.path()];
        index.loadFile(e.path());
        for (auto const& [key, infos] : index.packages) {
            for (auto const& info : infos) {
                auto s = fmt::format("{}@{}#{}", key, info.version, info.hash);
                pkgToInfo[s] = {e.path().string(), key, &info};
            }
        }
    }

    auto userChoosenPkgs = std::unordered_set<std::string>{};
    auto requiredPkgs    = std::unordered_set<std::string>{};

    for (auto p : *cli) {
        using Info = PackageIndex::Info;
        auto [key, info] = [&]() -> std::tuple<std::string, Info const*> {
            auto closeHits = std::map<std::string, std::tuple<std::string, Info const*>>{};
            for (auto const& e : std::filesystem::directory_iterator{path}) {
                if (e.path().extension() != ".db") continue;
                auto& index = indices[e.path()];
                for (auto const& [key, infos] : index.packages) {
                    if (infos.empty()) continue;
                    for (auto const& info : infos) {
                        auto s = fmt::format("{}@{}#{}", key, info.version, info.hash);
                        if (s == p) return {key, &info};
                    }
                    if (key == p) {
                        auto const& info = infos.back();
                        auto s = fmt::format("{}@{}#{}", key, info.version, info.hash);
                        closeHits[s] = {key, &info};
                    }
                }
            }
            if (closeHits.size() == 1) {
                return closeHits.begin()->second;
            }
            if (closeHits.empty()) throw std::runtime_error{"Could not find " + p};
            auto msg = "Ambigous: multiple results for " + p + "\n";
            for (auto [h, _] : closeHits) {
                msg += h + "\n";
            }
            throw std::runtime_error{msg};
        }();
        auto s = fmt::format("{}@{}#{}", key, info->version, info->hash);
        userChoosenPkgs.insert(s);
        requiredPkgs.insert(s);
        for (auto const& d : info->dependencies) {
            requiredPkgs.insert(d);
        }
    }

    // install all packages
    for (auto p : requiredPkgs) {
        if (istPkgs.contains(p + ".gar")) continue; // already installed
        auto iter = pkgToInfo.find(p);
        if (iter == pkgToInfo.end()) {
            throw std::runtime_error{"missing upstream source for " + p};
        }
        auto const& [path, key, info] = iter->second;
        auto config_path = std::filesystem::path{path};
        config_path.replace_extension();
        auto config = UpstreamConfig{};
        config.loadFile(config_path);

        if (!config.valid()) throw std::runtime_error{"invalid config " + config_path.string()};
        if (config.type == "ignore") continue;

        std::cout << "found " << p << " at " << path << "\n";
        if (config.type == "file") {
            p += ".gar.zst";
            auto source = std::filesystem::path{config.path} / p;
            auto dest   = getSlixConfigPath() / "packages" / p;
            std::filesystem::copy_file(source, dest, std::filesystem::copy_options::overwrite_existing);
            {
                auto d2 = dest;
                d2.replace_extension();
                auto call = fmt::format("zstd -q -d --rm {} -o {}", dest, d2);
                std::system(call.c_str());
            }

        } else if (config.type == "https") {
            p += ".gar.zst";
            auto source = std::filesystem::path{config.path} / p;
            auto dest   = getSlixConfigPath() / "packages" / p;
            //!TODO requires much better url encoding
            source = [](std::string input) -> std::string {
                std::string output;
                for (auto c : input) {
                    if (c == '#') {
                        output += "%23";
                    } else {
                        output += c;
                    }
                }
                return output;
            }(source);
            {
                auto call = fmt::format("curl -s {} -o {}", source, dest);
                if (cliVerbose) {
                    fmt::print("calling \"{}\"\n", call);
                }
                std::system(call.c_str());
            }
            {
                auto d2 = dest;
                d2.replace_extension();
                auto call = fmt::format("zstd -q -d --rm {} -o {}", dest, d2);
                std::system(call.c_str());
            }
        } else {
            throw std::runtime_error{"unknown upstream type " + config.type};
        }
    }
}
}
