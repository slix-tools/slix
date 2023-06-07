#include "utils.h"
#include "GarFuse.h"
#include "MyFuse.h"
#include "InteractiveProcess.h"

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
auto cliHelp = clice::Argument { .arg      = {"--help"},
                                 .desc     = "prints the help page",
                                 .cb       = []{ std::cout << clice::generateHelp(); exit(0); },
};

auto cliVerbose = clice::Argument{ .arg    = {"--verbose"},
                                   .desc   = "detailed description of what is happening",
};

auto cliPackages = clice::Argument{ .arg   = "-p",
                                    .desc  = "packages to initiate inside the environment",
                                    .value = std::vector<std::string>{},
};
}


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

auto getSlixRoots() -> std::vector<std::filesystem::path> {
    auto ptr = std::getenv("SLIX_ROOT");
    if (!ptr) throw std::runtime_error("unknown SLIX_ROOT");
    auto paths = std::vector<std::filesystem::path>{};
    for (auto part : std::views::split(std::string_view{ptr}, ':')) {
        paths.emplace_back(std::string_view{part.begin(), part.size()});
    }
    return paths;
}

void app() {
    auto slixRoots = getSlixRoots();
    auto input = std::vector<std::filesystem::path>{};
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
        auto const& fuse = layers.emplace_back(path, cliVerbose);
        for (auto const& d : fuse.dependencies) {
            packages.push_back(d);
        }
    }

    static auto onExit = std::function<void(int)>{};
    std::signal(SIGINT, [](int signal) { if (onExit) { onExit(signal); } });

    auto fuseFS = MyFuse{std::move(layers), cliVerbose};
    onExit = [&](int) {
        fuseFS.close();
    };
    fuseFS.loop();
}

int main(int argc, char** argv) {
    try {
        if (auto failed = clice::parse(argc, argv); failed) {
            std::cerr << "parsing failed: " << *failed << "\n";
            return 1;
        }
        if (auto ptr = std::getenv("CLICE_COMPLETION"); ptr) {
            return 0;
        }
        app();
    } catch (std::exception const& e) {
        std::cerr << "error: " << e.what() << "\n";
    }
}
