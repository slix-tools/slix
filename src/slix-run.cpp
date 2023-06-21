#include "App.h"
#include "GarFuse.h"
#include "MyFuse.h"
#include "slix.h"
#include "utils.h"
#include "PackageIndex.h"

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


void app() {
    auto app = App{
        .verbose = cliVerbose,
    };

    auto istPkgs        = app.installedPackages();
    auto indices        = app.loadPackageIndices();

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

    mountAndWait(clice::argv0, mountPoint, *cli, cliVerbose);

    auto cmd = *cliCommand;

    // scan for first entry point (if cmd didn't set any thing)
    if (cmd.empty()) {
        auto slixPkgPaths = std::vector{getSlixStatePath() / "packages"};
        for (auto input : *cli) {
            // find name of package
            auto [fullName, info] = indices.findInstalled(input, istPkgs);
            if (fullName.empty()) {
                throw error_fmt{"can not find any installed package for {}", input};
            }
            // find package location
            auto path = searchPackagePath(slixPkgPaths, fullName + ".gar");
            auto fuse = GarFuse{path, false};
            cmd = fuse.defaultCmd;
            if (!cmd.empty()) break;
        }
    }

    if (cmd.empty()) {
        throw std::runtime_error{"no command given"};
    }

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
        {"SLIX_ROOT", mountPoint},
    };

    execute(cmd, envp, /*.verbose=*/cliVerbose, /*.keepEnv=*/true);
    exit(127);
}
}
