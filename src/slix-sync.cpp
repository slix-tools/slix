// SPDX-FileCopyrightText: 2023 S. G. Gottlieb <info.simon@gottliebtfreitag.de>
// SPDX-License-Identifier: AGPL-3.0-only

#include "App.h"
#include "slix.h"
#include "utils.h"
#include "PackageIndex.h"
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
auto cli = clice::Argument{ .args   = {"-S", "sync"},
                            .desc   = "synchronize packages and environments",
                            .cb     = app,
};

auto cliUpdate = clice::Argument{ .parent = &cli,
                                  .args   = {"--update", "-u"},
                                  .desc   = "update the sources of all stores"
};

auto cliInstall = clice::Argument{ .parent = &cli,
                                   .args   = {"--install", "-i"},
                                   .desc   = "downloads and install packages or environment files",
                                   .value  = std::vector<std::string>{},
};

auto cliRemove = clice::Argument{ .parent = &cli,
                                   .args   = {"--remove", "-r"},
                                   .desc   = "remove packages or environment files",
                                   .value  = std::vector<std::string>{},
};

auto cliSearch = clice::Argument{ .parent = &cli,
                                  .args   = {"--search", "-s"},
                                  .desc   = "search for packages",
                                  .value  = std::vector<std::string>{},
};

auto cliDependencies = clice::Argument{ .parent = &cliSearch,
                                        .args   = {"--dependencies", "-d"},
                                        .desc   = "list all dependencies",
};
auto cliBrief = clice::Argument{ .parent = &cliSearch,
                                 .args   = {"--brief", "-b"},
                                 .desc   = "keep output brief"
};

auto cliList = clice::Argument{ .parent = &cli,
                                .args   = {"--list", "-l"},
                                .desc   = "list installed packages",
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
            // Check if it is a environment file
            if (exists(std::filesystem::path(i))) {
                i = absolute(std::filesystem::path(i)).string();
                auto envs = stores.getInstalledEnvironmentFiles();
                // upgrade environment
                if (envs.contains(i)) {
                    auto installedPackages = stores.listPackagesFromEnvironment(i);
                    auto packages          = readSlixEnvFile(i);
                    bool upgraded = false;
                    for (auto p : packages) {
                        if (installedPackages.contains(p)) {
                            installedPackages.erase(p);
                        } else {
                            stores.install(p, i);
                            upgraded = true;
                        }
                    }
                    for (auto p : installedPackages) {
                        upgraded = true;
                        stores.remove(p, i);
                    }
                    if (upgraded) {
                        fmt::print("environment {} was installed, applied upgrades\n", i);
                    } else {
                        fmt::print("environment {} was installed, nothing to do\n", i);
                    }
                } else { // install new environment
                    auto packages = readSlixEnvFile(i);
                    for (auto p : packages) {
                        stores.install(p, i);
                    }
                    fmt::print("environment {} installed\n", i);
                }
            } else { // Otherwise assume its a package name
                bool newlyInstalled = stores.install(i, "");
                if (!newlyInstalled) {
                    fmt::print("package {} was already installed\n", i);
                } else {
                    fmt::print("package {} installed\n", i);
                }
            }
        }
        stores.save(getSlixStatePath() / "stores.yaml");
    } else if (cliRemove) {
        // Load all stores, and check if it is already available
        auto stores = Stores{storePath};
        for (auto i : *cliRemove) {
            // Check if it is a environment file
            if (exists(std::filesystem::path(i))) {
                i = absolute(std::filesystem::path(i)).string();
                auto envs = stores.getInstalledEnvironmentFiles();
                if (!envs.contains(i)) {
                    fmt::print("environment {} not installed\n", i);
                    continue;
                }
                auto packages = readSlixEnvFile(i);
                for (auto p : packages) {
                    stores.remove(p, i);
                }
                fmt::print("environment {} removed\n", i);
            } else { // Otherwise assume its a package name
                bool removed = stores.remove(i, "");
                if (!removed) {
                    fmt::print("package {} not installed\n", i);
                } else {
                    fmt::print("package {} removed\n", i);
                }
            }
        }
        stores.save(getSlixStatePath() / "stores.yaml");

    } else if (cliSearch) {
        // Go through store by store and search
        auto stores = Stores{storePath};
        for (auto name : *cliSearch) {
            auto names = stores.findExactName(name);
            for (auto n : names) {
                if (cliBrief) {
                    fmt::print("{}\n", n);
                } else {
                    fmt::print("{}{}\n", n, stores.isInstalled(n)?" (installed)":"");
                }
                if (cliDependencies) {
                    auto [knownList, installedStore] = stores.findExactPattern(n);
                    if (installedStore) {
                        auto deps = installedStore->loadPackageIndex().findDependencies(n);
                        for (auto d : std::set<std::string>{deps.begin(), deps.end()}) {
                            if (cliBrief) {
                                fmt::print("{}\n", d);
                            } else {
                                bool isInstalled = stores.isInstalled(d);
                                fmt::print("- {}{}\n", d, isInstalled?" (installed)":"");
                            }
                        }

                    }
                }
            }
            if (names.empty()) {
                auto names = stores.findWithPrefix(name);
                for (auto n : names) {
                    if (cliBrief) {
                        fmt::print("{}\n", n);
                    } else {
                        fmt::print("{}{}\n", n, stores.isInstalled(n)?" (installed)":"");
                    }
                    if (cliDependencies) {
                        auto [knownList, installedStore] = stores.findExactPattern(n);
                        if (installedStore) {
                            auto deps = installedStore->loadPackageIndex().findDependencies(n);
                            for (auto d : std::set<std::string>{deps.begin(), deps.end()}) {
                                if (cliBrief) {
                                    fmt::print("{}\n", d);
                                } else {
                                    bool isInstalled = stores.isInstalled(d);
                                    fmt::print("- {}{}\n", d, isInstalled?" (installed)":"");
                                }
                            }

                        }
                    }
                }
            }
        }
    } else if (cliList) {
        // Go through store by store and search
        auto stores = Stores{storePath};

        auto installedEnvironmentFiles = stores.getInstalledEnvironmentFiles();
        auto explicitInstalledPackages = stores.getExplicitInstalledPackages();

        if (explicitInstalledPackages.size() > 0) {
            fmt::print("explicit packages:\n");
            for (auto p : explicitInstalledPackages) {
                fmt::print("- {}\n", p);
            }
        }

        if (installedEnvironmentFiles.size() > 0) {
            fmt::print("environment files:\n");
            for (auto f : installedEnvironmentFiles) {
                fmt::print("- {}\n", f);
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
