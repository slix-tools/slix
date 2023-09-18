#include "GarFuse.h"
#include "MyFuse.h"
#include "PackageIndex.h"
#include "slix.h"
#include "utils.h"

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
                                    .args  = "-p",
                                    .desc  = "packages to initiate inside the environment",
                                    .value = std::vector<std::string>{},
};
auto cliMountPoint = clice::Argument{ .parent = &cli,
                                      .args = "--mount",
                                      .desc = "path to the mount point",
                                      .value = std::string{},
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
    if (!cliMountPoint) {
        throw std::runtime_error{"no mount point given"};
    }
    if (!std::filesystem::exists(*cliMountPoint)) {
        std::filesystem::create_directories(*cliMountPoint);
    }

    auto slixPkgPaths   = std::vector{getSlixStatePath() / "packages"};
    auto istPkgs        = installedPackages(slixPkgPaths);
    auto packageIndices = loadPackageIndices();

    auto requiredPackages = std::unordered_set<std::string>{};
    for (auto input : *cliPackages) {
        auto [fullName, info] = packageIndices.findInstalled(input, istPkgs);
        if (fullName.empty()) {
            throw std::runtime_error{"can not find any installed package for " + input};
        }
        requiredPackages.insert(fullName);
        for (auto const& d : info.dependencies) {
            if (!istPkgs.contains(d + ".gar")) {
                throw std::runtime_error{fmt::format("package {} requires {}, but {} couldn't be found", fullName, d, d)};
            }
            requiredPackages.insert(d);
        }
    }


    auto layers = std::vector<GarFuse>{};
    for (auto const& p : requiredPackages) {
        if (cliVerbose) {
            std::cout << "layer " << layers.size() << " - " << p << "\n";
        }
        auto path = searchPackagePath(slixPkgPaths, p + ".gar");
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
//        std::this_thread::sleep_for(std::chrono::milliseconds{1000000});
        fuseFS.close();
        std::filesystem::remove(tempMount);
    } else {
        static auto onExit = std::function<void(int)>{};
        if (cliFork) {
            if (fork() != 0) return;
            std::signal(SIGHUP, [](int) {}); // ignore hangup signal
            std::signal(SIGINT, [](int) {});
        } else {
            std::signal(SIGINT, [](int signal) { if (onExit) { onExit(signal); } });
        }
        std::signal(SIGUSR1, [](int signal) { if (onExit) { onExit(signal); } });


        auto fuseFS = MyFuse{std::move(layers), cliVerbose, *cliMountPoint, cliAllowOther};
        std::jthread thread;
        onExit = [&](int) {
            thread = std::jthread{[&]() {
                std::this_thread::sleep_for(std::chrono::milliseconds{100});
                fuseFS.close();
            }};
        };
        fuseFS.loop();
    }
}
}
