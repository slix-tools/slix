#include "slix.h"
#include "utils.h"
#include "PackageIndex.h"

#include <clice/clice.h>
#include <filesystem>
#include <iostream>
#include <set>

namespace {
void app();
auto cli = clice::Argument{ .arg    = "search",
                            .desc   = "search locally for available slix packages",
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

    for (auto pattern : *cli) {
        for (auto const& e : std::filesystem::directory_iterator{path}) {
            auto index = PackageIndex{};
            index.loadFile(e.path());
            for (auto const& [key, infos] : index.packages) {
                if (key.starts_with(pattern) and !infos.empty()) {
                    auto const& info = infos.back();
                    fmt::print("{}@{}#{}\n", key, info.version, info.hash);
                }
            }
        }
    }
}
}
