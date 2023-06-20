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
            throw std::runtime_error{"can find any installed package for " + input};
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

    static auto onExit = std::function<void(int)>{};
    if (cliFork) {
        if (fork() != 0) return;
        std::signal(SIGHUP, [](int) {}); // ignore hangup signal
        std::signal(SIGINT, [](int) {});
    } else {
        std::signal(SIGINT, [](int signal) { if (onExit) { onExit(signal); } });
    }
    std::signal(SIGUSR1, [](int signal) { if (onExit) { onExit(signal); } });


    auto fuseFS = MyFuse{std::move(layers), cliVerbose, *cliMountPoint};
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
