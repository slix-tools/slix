// SPDX-FileCopyrightText: 2023 S. G. Gottlieb <info.simon@gottliebtfreitag.de>
// SPDX-License-Identifier: AGPL-3.0-only

#include "GarFuse.h"
#include "MyFuse.h"
#include "PackageIndex.h"
#include "slix.h"
#include "utils.h"
#include "Stores.h"

#include <atomic>
#include <clice/clice.h>
#include <csignal>
#include <filesystem>
#include <functional>
#include <iostream>
#include <memory>
#include <sstream>
#include <thread>

namespace {
void app();
auto cli = clice::Argument{ .args   = "mount",
                            .desc   = "mounts a shell environment",
                            .cb     = app,
};

auto cliPackages = clice::Argument{ .parent = &cli,
                                    .args  = {"-p", "--packages"},
                                    .desc  = "packages to initiate inside the environment",
                                    .value = std::vector<std::string>{},
};
auto cliMountPoint = clice::Argument{ .parent = &cli,
                                      .args = "--mount",
                                      .desc = "path to the mount point",
                                      .value = std::string{},
                                      .tags = {"required"},
};
auto cliFork = clice::Argument{ .parent = &cli,
                                .args = "--fork",
                                .desc = "fork program to run in the background (also ignores SIGHUP)",
};

auto cliAllowOther = clice::Argument{ .parent = &cli,
                                .args = "--allow_other",
                                .desc = "Allows other users to access to the mounted paths",
};

auto cliUnpack = clice::Argument{ .parent = &cli,
                                  .args = "--unpack",
                                  .desc = "instead of mounting, this will unpack/copy the files to the mount point"
};

void app() {
    if (!std::filesystem::exists(*cliMountPoint)) {
        std::filesystem::create_directories(*cliMountPoint);
    }

    storeInit();
    auto storePath = getSlixConfigPath() / "stores";

    // Go through store by store and list gather all requested packages and their dependencies
    // Paths to the required packages
    auto requiredPackages = std::unordered_set<std::filesystem::path>{};

    auto stores = Stores{storePath};
    for (auto name : *cliPackages) {
        auto names = stores.findExactName(name);
        if (names.size() == 0) {
            throw error_fmt{"package {} not found", name};
        } else if (names.size() > 1) {
            throw error_fmt{"multiple packages with name {} found: {}", name, fmt::join(names, ", ")};
        }
        auto n = names[0];
        auto [knownList, installedStore] = stores.findExactPattern(n);
        if (!installedStore) {
            throw error_fmt{"package {} not installed", n};
        }
        requiredPackages.insert(installedStore->getPackagePath(n));
        for (auto d : installedStore->loadPackageIndex().findDependencies(n)) {
            requiredPackages.insert(installedStore->getPackagePath(d));
        }
    }

    auto layers = std::vector<GarFuse>{};
    for (auto const& path : requiredPackages) {
        layers.emplace_back(path, cliVerbose);
    }

    if (cliUnpack) {
        auto tempMount = *cliMountPoint + "/slix-temporary-mount-fs";
        std::filesystem::create_directories(tempMount);
        auto fuseFS = MyFuse{std::move(layers), cliVerbose, tempMount, cliAllowOther};
        auto thread = std::jthread{[&]() {
            fuseFS.loop();
        }};
        for (auto const& dir_entry : std::filesystem::directory_iterator{tempMount}) {
            if (dir_entry.path() == tempMount + "/slix-lock") continue;
            std::system(fmt::format("cp -ar \"{}\" \"{}\"", dir_entry.path(), *cliMountPoint).c_str());
        }
        fuseFS.close();
        std::filesystem::remove(tempMount);
    } else {
        static auto onExit = std::function<void(int)>{};

        auto fuseFS = MyFuse{std::move(layers), cliVerbose, *cliMountPoint, cliAllowOther};
        std::jthread thread;
        onExit = [&](int) {
            thread = std::jthread{[&]() {
                std::this_thread::sleep_for(std::chrono::milliseconds{100});
                fuseFS.close();
            }};
        };

        if (cliFork) {
            if (fork() != 0) {
                fuseFS.mountPoint = "";
                return;
            }
            std::signal(SIGHUP, [](int) {}); // ignore hangup signal
            std::signal(SIGINT, [](int) {});
        } else {
            std::signal(SIGINT, [](int signal) { if (onExit) { onExit(signal); } });
        }
        std::signal(SIGUSR1, [](int signal) { if (onExit) { onExit(signal); } });

        fuseFS.loop();
    }
}
}
