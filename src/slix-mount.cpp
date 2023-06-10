#include "utils.h"
#include "GarFuse.h"
#include "MyFuse.h"
#include "slix.h"

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
auto cli = clice::Argument{ .arg    = "mount",
                                   .desc   = "mounts a shell environment",
                                   .cb     = app,
};

auto cliPackages = clice::Argument{ .parent = &cli,
                                    .arg   = "-p",
                                    .desc  = "packages to initiate inside the environment",
                                    .value = std::vector<std::string>{},
};
auto cliMountPoint = clice::Argument{ .parent = &cli,
                                      .arg  = "--mount",
                                      .desc = "path to the mount point",
                                      .value = std::string{},
};


auto searchPackagePath(std::vector<std::filesystem::path> const& slixRoots, std::string const& name) -> std::filesystem::path {
    for (auto p : slixRoots) {
        for (auto pkg : std::filesystem::directory_iterator{p}) {
            auto filename = pkg.path().filename();
            if (filename.string().starts_with(name + "@")) {
                return pkg.path();
            } else if (filename == name) {
                return pkg.path();
            }
        }
    }
    return {};
}

void app() {
    if (!cliMountPoint) {
        throw std::runtime_error{"no mount point given"};
    }

    if (fork() != 0) return;


    {
        auto slixRoots = getSlixRoots();
        auto layers = std::vector<GarFuse>{};
        auto packages = *cliPackages;
        auto addedPackages = std::unordered_set<std::string>{};
        while (!packages.empty()) {
            auto input = std::filesystem::path{packages.back()};
            packages.pop_back();
            if (addedPackages.contains(input)) continue;
            addedPackages.insert(input);
            if (cliVerbose) {
                std::cout << "layer " << layers.size() << " - ";
            }
            auto path = searchPackagePath(slixRoots, input);
            if (path.empty()) {
                auto msg = std::string{"Could not find package \""} + input.string() + "\". Searched in paths: ";
                for (auto r : slixRoots) {
                    msg += r.string() + ", ";
                }
                if (slixRoots.size()) {
                    msg = msg.substr(0, msg.size()-2);
                }
                throw std::runtime_error(msg);
            }
            auto const& fuse = layers.emplace_back(path, cliVerbose);
            for (auto const& d : fuse.dependencies) {
                packages.push_back(d);
            }
        }

        std::signal(SIGHUP, [](int) {}); // ignore hangup signal
        static auto onExit = std::function<void(int)>{};
        std::signal(SIGINT, [](int signal) { if (onExit) { onExit(signal); } });

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
    exit(0);
}
}
