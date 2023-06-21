#include "App.h"
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
auto cli = clice::Argument{ .args   = "sync",
                            .desc   = "synchronizes a package and its dependencies",
                            .value  = std::vector<std::string>{},
                            .cb     = app,
};

void app() {
    auto app = App{
        .verbose = cliVerbose,
    };

    auto istPkgs        = app.installedPackages();
    auto indices        = app.loadPackageIndices();

    auto requiredPkgs    = std::unordered_set<std::string>{};

    for (auto p : *cli) {
        requiredPkgs.merge(indices.findDependencies(p));
    }

    // install all packages
    for (auto p : requiredPkgs) {
        if (istPkgs.contains(p + ".gar")) continue; // already installed

        auto config = indices.findPackageConfig(p);

        if (cliVerbose) {
            fmt::print("found {} at {}\n", p, config.path);
        }

        app.installPackage(p, config);
    }
}
}
