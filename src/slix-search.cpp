#include "slix.h"
#include "utils.h"
#include "PackageIndex.h"

#include <clice/clice.h>
#include <filesystem>
#include <iostream>
#include <set>

namespace {
void app();
auto cli = clice::Argument{ .args   = "search",
                            .desc   = "search locally for available slix packages",
                            .value  = std::vector<std::string>{},
                            .cb     = app,
};

void app() {
    auto slixPkgPaths   = std::vector{getSlixStatePath() / "packages"};
    auto istPkgs        = installedPackages(slixPkgPaths);
    auto packageIndices = loadPackageIndices();

    for (auto pattern : *cli) {
        auto opt = packageIndices.findLatest(pattern);
        if (!opt) continue;
        auto [path, index, key, info] = *opt;
        fmt::print("{}@{}#{}\n", key, info->version, info->hash);
    }
}
}
