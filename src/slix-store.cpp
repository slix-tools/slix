#include "App.h"
#include "slix.h"
#include "utils.h"
#include "PackageIndex.h"
#include "PackageSupervisor.h"
#include "UpstreamConfig.h"
#include "Stores.h"

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
auto cli = clice::Argument{ .args   = {"store"},
                            .desc   = "information about stores",
                            .cb     = app,
};

auto cliUpdate = clice::Argument{ .parent = &cli,
                                  .args   = {"--update"},
                                  .desc   = "update the sources of all stores"
};

auto cliInstall = clice::Argument{ .parent = &cli,
                                   .args   = {"--install"},
                                   .desc   = "downloads a package",
                                   .value  = std::vector<std::string>{},
};

auto cliSearch = clice::Argument{ .parent = &cli,
                                  .args   = {"--search"},
                                  .desc   = "search for packages",
                                  .value  = std::vector<std::string>{},
};

auto cliDependencies = clice::Argument{ .parent = &cliSearch,
                                        .args   = {"-d"},
                                        .desc   = "list all dependencies",
};



void app() {
    auto app = App {
        .verbose = cliVerbose,
    };
    app.init();

    storeInit();
    auto storePath = getSlixConfigPath() / "stores";

    if (cliUpdate) {
        for (auto const& e : std::filesystem::directory_iterator{storePath}) {
            auto store = Store{e.path()};
            fmt::print("updating store {} ({})\n", store.name, getSlixStatePath() / store.name);
            store.update();
        }
    } else if (cliInstall) {
        // Load all stores, and check if it is already available
        auto stores = Stores{storePath};
        for (auto i : *cliInstall) {
            stores.install(i, /*.explictMarked=*/true);
        }
        stores.save(getSlixStatePath() / "stores.yaml");
    } else if (cliSearch) {
        // Go through store by store and search
        auto stores = Stores{storePath};
        for (auto name : *cliSearch) {
            auto names = stores.findExactName(name);
            for (auto n : names) {
                fmt::print("{}{}\n", n, stores.isInstalled(n)?" (installed)":"");
                if (cliDependencies) {
                    auto [knownList, installedStore] = stores.findExactPattern(n);
                    if (installedStore) {
                        auto deps = installedStore->loadPackageIndex().findDependencies(n);
                        for (auto d : std::set<std::string>{deps.begin(), deps.end()}) {
                            fmt::print("d: {}\n", d);
                        }

                    }
                }
            }
        }
    } else {
        auto packageNames = std::set<std::string>{};
        for (auto const& e : std::filesystem::directory_iterator{storePath}) {
            auto store = Store{e.path()};
            fmt::print("store at: {}\n", getSlixStatePath() / store.name);
            auto index = store.loadPackageIndex();
            auto const& s = store.config.source;
            fmt::print("  - {} ({}) available packages {}\n", s.url, s.type, index.packages.size());
            for (auto const& [key, list] : index.packages) {
                packageNames.insert(key);
            }
        }
        fmt::print("  total packages {}\n", packageNames.size());
    }
}
}
