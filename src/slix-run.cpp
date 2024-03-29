// SPDX-FileCopyrightText: 2023 S. G. Gottlieb <info.simon@gottliebtfreitag.de>
// SPDX-License-Identifier: AGPL-3.0-only

#include "MyFuse.h"
#include "slix.h"
#include "utils.h"
#include "PackageIndex.h"
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
auto cli = clice::Argument{ .args   = "run",
                            .desc   = "starts a slix environment with specified packages",
                            .value  = std::vector<std::string>{},
                            .cb     = app,
};
auto cliCommand = clice::Argument { .parent = &cli,
                                    .args  = {"-c"},
                                    .desc  = "program to execute inside the shell",
                                    .value = std::vector<std::string>{},
};
auto cliMountPoint = clice::Argument{ .parent = &cli,
                                      .args = "--mount",
                                      .desc = "path to the mount point or if already in use, reuse",
                                      .value = std::string{},
};

auto cliMountOptions  = clice::Argument{ .parent = &cli,
                                         .args  = {"-o", "--options"},
                                         .desc  = "mount options",
                                         .value = std::vector<std::string>{},
};

auto cliStack = clice::Argument{ .parent = &cli,
                                 .args   = "--stack",
                                 .desc   = "Will add paths to PATH instead of overwritting, allows stacking behavior",
};

void app() {
    storeInit();
    auto storePath = getSlixConfigPath() / "stores";

    auto mountPoint = [&]() -> std::string {
        if (cliMountPoint) {
            if (!std::filesystem::exists(*cliMountPoint)) {
                std::filesystem::create_directory(*cliMountPoint);
            }
            return *cliMountPoint;
        } else {
            return create_temp_dir().string();
        }
    }();

    auto requestedPackages = std::vector<std::string>{};
    for (auto i : *cli) {
        // Check if it a file
        if (exists(std::filesystem::path(i))) {
            // Check if it is package on the command line
            if (i.ends_with(".gar")) {
                requestedPackages.push_back(absolute(std::filesystem::path(i)).string());

            // Assuming it is an environment file
            } else {
                i = absolute(std::filesystem::path(i)).string();
                auto packages = readSlixEnvFile(i);
                for (auto p : packages) {
                    requestedPackages.push_back(p);
                }
            }
        } else { // Other wise assume its a package name
            requestedPackages.push_back(i);
        }
    }

    auto handle = mountAndWait(clice::argv0, mountPoint, requestedPackages, cliVerbose, *cliMountOptions);

    auto stores = Stores{storePath};
    auto installedPackagePaths = std::unordered_map<std::string, std::filesystem::path>{};
    for (auto i : stores.installedPackages) {
        installedPackagePaths.try_emplace(i, stores.getPackagePath(i));
    }

    auto cmd = [&]() -> std::vector<std::string> {
        if (cliCommand->size()) return *cliCommand;
        auto fullNames = std::vector<std::string>{};
        for (auto requested_name : requestedPackages) {
            auto [store, name] = stores.findNewestPackageByName(requested_name, /*installed = */ true);
            if (name.empty()) {
                throw error_fmt{"package {} not found", requested_name};
            }
            if (stores.isInstalled(name)) {
                fullNames.push_back(name);
            } else {
                throw error_fmt{"package {} is not installed", requested_name};
            }
        }
        return scanDefaultCommand(fullNames, installedPackagePaths);
    }();

    // set argv variables
    cmd.insert(cmd.begin(), "/usr/bin/env");

    // Set environment variables
    auto PATH = getPATH();
    if (PATH.empty() || !cliStack) {
        PATH = mountPoint + "/usr/bin";
    } else {
        PATH = mountPoint + "/usr/bin:" + PATH;
    }

    auto envp = std::map<std::string, std::string> {
        {"PATH", PATH},
    };

    execute(cmd, envp, /*.verbose=*/cliVerbose, /*.keepEnv=*/true);
    exit(127);
}
}
