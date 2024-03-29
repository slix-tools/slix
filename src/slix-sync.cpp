// SPDX-FileCopyrightText: 2023 S. G. Gottlieb <info.simon@gottliebtfreitag.de>
// SPDX-License-Identifier: AGPL-3.0-only

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

auto cliRefreshIndex = clice::Argument{ .parent = &cli,
                                       .args   = {"--refresh", "-y"},
                                       .desc   = "refresh the upstream list of packages",
};

auto cliUpdate = clice::Argument{ .parent = &cli,
                                  .args   = {"--update", "-u"},
                                  .desc   = "update explicit installed packages, if environment file is given replace packages with newer packages",
                                  .value  = std::vector<std::string>{},
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

auto cliSearchAll = clice::Argument{ .parent = &cliSearch,
                                     .args   = {"--all", "-a"},
                                     .desc   = "display all versions"
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
    storeInit();
    auto storePath = getSlixConfigPath() / "stores";

    if (cliRefreshIndex) {
        for (auto const& e : std::filesystem::directory_iterator{storePath}) {
            auto store = Store{e.path()};
            if (cliVerbose) {
                fmt::print("refresh store {} ({})\n", store.name, getSlixStatePath() / store.name);
            }
            store.update();
        }
    }


    if (cliUpdate) {
        auto stores = Stores{storePath};
        for (auto i : *cliUpdate) {
            if (exists(std::filesystem::path(i))) {
                i = absolute(std::filesystem::path(i)).string();
                auto envFile  = readSlixEnvFile(i);
                auto packages = envFile.packages;
                bool changes = false;
                for (auto p : packages) {
                    auto newName = stores.findUpgrade(p);
                    if (newName != p) {
                        envFile.replacePackage(p, newName);
                        fmt::print("upgrading {} -> {}\n", p, newName);
                        changes = true;
                    }
                }
                if (changes) {
                    saveSlixEnvFile(envFile, i);
                } else {
                    fmt::print("nothing upgraded in {}\n", i);
                }

            } else {
                throw error_fmt{"invalid environment file {}", i};
            }
        }
    }

    if (cliInstall) {
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
        auto installedEnvironmentFiles = stores.getInstalledEnvironmentFiles();
        auto explicitInstalledPackages = stores.getExplicitInstalledPackages();

        for (auto i : *cliRemove) {
            auto ai = absolute(std::filesystem::path{i});
            if (installedEnvironmentFiles.contains(ai)) {
                auto installedPackages = stores.listPackagesFromEnvironment(ai);
                for (auto i2 : installedPackages) {
                    stores.remove(i2, ai);
                }
                fmt::print("removed environment {}\n", ai);
            } else {
                auto names = stores.findExactName(i, /*.onlyNewest=*/false);
                size_t count{};
                if (!names.empty()) {
                    for (auto n : names) {
                        if (explicitInstalledPackages.contains(n)) {
                            count += 1;
                            stores.remove(n, "");
                        }
                    }
                    if (count > 0) {
                        fmt::print("removed {} ({} package{})\n", i, count, (count==1)?"":"s");
                    } else {
                        fmt::print("package {} is not installed\n", i);
                    }
                }
            }
        }
        stores.save(getSlixStatePath() / "stores.yaml");

    } else if (cliSearch) {
        // Go through store by store and search
        auto stores = Stores{storePath};

        auto allreadyPrinted = std::unordered_set<std::string>{};

        auto printEntry = [&](std::string packageName) {
            if (allreadyPrinted.contains(packageName)) return;
            allreadyPrinted.insert(packageName);
            if (cliBrief) {
                fmt::print("{}\n", packageName);
            } else {
                bool isInstalled = stores.isInstalled(packageName);
                if (isInstalled) {
                    fmt::print("- {} (installed)\n", packageName);
                } else {
                    auto nameOnly = packageName.substr(0, packageName.find('@'));
                    bool isInstalled = stores.isAnyVersionInstalled(nameOnly);
                    if (isInstalled) {
                        fmt::print("- {} (older version installed)\n", packageName);
                    } else {
                        fmt::print("- {}\n", packageName);
                    }
                }
            }
        };

        for (auto search_name : *cliSearch) {
            auto names = stores.findWithPrefix(search_name, !cliSearchAll);
            for (auto n : names) {
                if (cliDependencies) {
                    auto [knownList, installedStore] = stores.findExactPattern(n);
                    if (installedStore) {
                        auto deps = installedStore->loadPackageIndex().findDependencies(n);
                        for (auto d : std::set<std::string>{deps.begin(), deps.end()}) {
                            printEntry(d);
                        }
                    }
                } else {
                    printEntry(n);
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
                if (f.empty()) continue;
                fmt::print("- {}\n", f);
            }
        }
    }
}
}
