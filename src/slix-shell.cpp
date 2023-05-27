#include "utils.h"
#include "GarFuse.h"
#include "MyFuse.h"
#include "InteractiveProcess.h"

#include <atomic>
#include <csignal>
#include <filesystem>
#include <functional>
#include <iostream>
#include <memory>
#include <sstream>
#include <thread>


std::atomic_bool finish{false};

auto searchPackagePath(std::vector<std::filesystem::path> const& slixRoots, std::string const& name) -> std::filesystem::path {
    for (auto p : slixRoots) {
        for (auto pkg : std::filesystem::directory_iterator{p}) {
            auto filename = pkg.path().filename();
            if (filename.string().starts_with(name + "@")) {
                return pkg.path();
            }
        }
    }
    return name;
}

int main(int argc, char** argv) {
    if (argc < 1) {
        throw std::runtime_error{"need at least 1 parameters"};
    }
    auto slixRoots = [&]() -> std::vector<std::filesystem::path> {
        auto ptr = std::getenv("SLIX_ROOT");
        if (!ptr) throw std::runtime_error("unknown SLIX_ROOT");
        auto paths = std::vector<std::filesystem::path>{};
        for (auto part : std::views::split(std::string_view{ptr}, ':')) {
            paths.emplace_back(std::string_view{part.begin(), part.size()});
        }
        return paths;
    }();
    auto input = std::vector<std::filesystem::path>{};
    auto layers = std::vector<GarFuse>{};
    auto packages = std::vector<std::string>{};
    auto addedPackages = std::unordered_set<std::string>{};
    for (size_t i{1}; i < argc; ++i) {
        packages.emplace_back(argv[i]);
    }
    while (!packages.empty()) {
        auto input = std::filesystem::path{packages.back()};
        packages.pop_back();
        if (addedPackages.contains(input)) continue;
        addedPackages.insert(input);
        std::cout << "layer " << layers.size() << " - ";
        auto path = searchPackagePath(slixRoots, input);
        auto const& fuse = layers.emplace_back(path);
        for (auto const& d : fuse.dependencies) {
            packages.push_back(d);
        }
    }

    static auto onExit = std::function<void(int)>{[](int){}};

    std::signal(SIGINT, [](int signal) {
        onExit(signal);
    });

    auto fuseFS = MyFuse{std::move(layers)};
    auto runningFuse = std::jthread{[&]() {
        while (!finish) {
            fuseFS.loop();
        }
    }};
    try {
        auto envp = std::vector<std::string>{};
        envp.push_back("PATH=" + fuseFS.mountPoint.string() + "/usr/bin");
        envp.push_back("LD_LIBRARY_PATH=" + fuseFS.mountPoint.string() + "/usr/lib");

        auto ip = process::InteractiveProcess{std::vector<std::string>{fuseFS.mountPoint.string() + "/usr/bin/bash", "--norc", "--noprofile"}, envp};
        onExit = [&](int signal) {
            ip.sendSignal(signal);
        };
    } catch(std::exception const& e) {
        std::cout << "error: " << e.what() << "\n";
    }
    onExit = [](int) {};
    finish = true;
    fuseFS.close();
    runningFuse.join();
}
