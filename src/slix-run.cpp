// SPDX-FileCopyrightText: 2023 S. G. Gottlieb <info.simon@gottliebtfreitag.de>
// SPDX-License-Identifier: AGPL-3.0-only

#include "App.h"
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
auto cliStack = clice::Argument{ .parent = &cli,
                                 .args   = "--stack",
                                 .desc   = "Will add paths to PATH instead of overwritting, allows stacking behavior",
};
auto cliAllowOther = clice::Argument{ .parent = &cli,
                                .args = "--allow_other",
                                .desc = "Allows other users to access to the mounted paths",
};

void app() {
    auto app = App{
        .verbose = cliVerbose,
    };
    app.init();

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
        // Check if it is a environment file
        if (exists(std::filesystem::path(i))) {
            i = absolute(std::filesystem::path(i)).string();
            auto packages = readSlixEnvFile(i);
            for (auto p : packages) {
                requestedPackages.push_back(p);
            }
        } else { // Other wise assume its a package name
            requestedPackages.push_back(i);
        }
    }



    auto handle = mountAndWait(clice::argv0, mountPoint, requestedPackages, cliVerbose, cliAllowOther);

    auto stores = Stores{storePath};
    auto installedPackagePaths = std::unordered_map<std::string, std::filesystem::path>{};
    for (auto i : stores.installedPackages) {
        installedPackagePaths.try_emplace(i, stores.getPackagePath(i));
    }

    auto cmd = [&]() -> std::vector<std::string> {
        if (cliCommand->size()) return *cliCommand;
        auto fullNames = std::vector<std::string>{};
        for (auto name : requestedPackages) {
            auto names = stores.findExactName(name);
            bool added = false;
            for (auto n : names) {
                if (stores.isInstalled(n)) {
                    fullNames.push_back(n);
                    added = true;
                    break;
                }
            }
            if (!added) {
                throw error_fmt{"unknown package {}", name};
            }
        }
        return scanDefaultCommand(fullNames, installedPackagePaths);
    }();

    // set argv variables
    cmd.insert(cmd.begin(), "/usr/bin/env");

    // Set environment variables
    auto PATH = app.getPATH();
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
