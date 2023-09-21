#include "App.h"
#include "slix.h"
#include "utils.h"
#include "PackageIndex.h"
#include "PackageSupervisor.h"
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
auto cli = clice::Argument{ .args   = {"query", "-Q"},
                            .desc   = "list packages that are already installed",
                            .value  = std::string{},
                            .cb     = app,
};

auto cliAll      = clice::Argument{ .args   = {"-a", "--all"},
                                    .desc   = "list all files (not only explicit marked ones)",
};


void app() {
    auto app = App {
        .verbose = cliVerbose,
    };
    app.init();

    auto istPkgs = app.installedPackages();
    auto ordered = std::set<std::string>{istPkgs.begin(), istPkgs.end()};

    auto supervisor = PackageSupervisor{};
    if (exists(getSlixConfigPath() / "config.yaml")) {
        supervisor.loadFile(getSlixConfigPath() / "config.yaml");
    }

    auto trackedEnvFiles = std::set<std::string>{};

    fmt::print("installed packages:\n");
    // installed packages
    for (auto i : ordered) {
        i = i.substr(0, i.size()-4);
        if (cliAll) {
            fmt::print("  - {}\n", i);
        } else {
            auto iter = supervisor.packages.find(i);
            if (iter != supervisor.packages.end()) {
                if (iter->second.explicitMarked) {
                    fmt::print("  - {}\n", i);
                }
                for (auto e : iter->second.environments) {
                    trackedEnvFiles.insert(e);
                }
            }
        }
    }

    fmt::print("registered environments:\n");
    for (auto t : trackedEnvFiles) {
        fmt::print("  - {}\n", t);
    }
}
}
