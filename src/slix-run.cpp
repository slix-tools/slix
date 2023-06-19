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
auto cli = clice::Argument{ .arg    = "run",
                            .desc   = "starts a slix environment with specified packages",
                            .value  = std::vector<std::string>{},
                            .cb     = app,
};

auto cliCommand = clice::Argument { .parent = &cli,
                                    .arg   = {"-c"},
                                    .desc  = "program to execute inside the shell",
                                    .value = std::vector<std::string>{},
};

auto cliMountPoint = clice::Argument{ .parent = &cli,
                                      .arg  = "--mount",
                                      .desc = "path to the mount point or if already in use, reuse",
                                      .value = std::string{},
};

auto cliStack = clice::Argument{ .parent = &cli,
                                 .arg    = "--stack",
                                 .desc   = "Will add paths to PATH instead of overwritting, allows stacking behavior",
};


void app() {
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

    auto slixPkgPaths   = getSlixPkgPaths();
    auto istPkgs        = installedPackages(slixPkgPaths);
    auto packageIndices = loadPackageIndices();

    auto cmd = *cliCommand;

    if (!std::filesystem::exists(std::filesystem::path{mountPoint} / "slix-lock")) {
        if (cliVerbose) {
            std::cout << "argv0: " << clice::argv0 << "\n";
            std::cout << "self-exe: " << std::filesystem::canonical("/proc/self/exe") << "\n";
        }
        auto binary = std::filesystem::path{clice::argv0};
        binary = std::filesystem::canonical("/proc/self/exe").parent_path().parent_path() / "bin" / binary.filename();

        auto call = binary.string();
        if (cliVerbose) call += " --verbose";
        call += " mount --fork --mount " + mountPoint + " -p";
        for (auto p : *cli) {
            call += " " + p;
        }
        if (cliVerbose) {
            std::cout << "call mount " << call << "\n";
        }
        std::system(call.c_str());
    }
    auto ifs = std::ifstream{};
    while (!ifs.is_open()) {
        std::this_thread::sleep_for(std::chrono::milliseconds{10}); //!TODO can we do this better to wait for slix-mount to finish?
        ifs.open(mountPoint + "/slix-lock");
    }

    // scan for first entry point (if cmd didn't set any thing)
    if (cmd.empty()) {
        for (auto input : *cli) {
            // find name of package
            auto [fullName, info] = [&]() -> std::tuple<std::string, PackageIndex::Info> {
                for (auto const& [path, index] : packageIndices.indices) {
                    for (auto const& [key, infos] : index.packages) {
                        for (auto const& info : infos) {
                            auto s = fmt::format("{}@{}#{}", key, info.version, info.hash);
                            if (key == input or s == input) {
                                if (istPkgs.contains(s + ".gar")) {
                                    return {s, info};
                                }
                            }
                        }
                    }
                }
                throw std::runtime_error{"can find any installed package for " + input};
            }();
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


    auto argv = std::vector<char const*>{"/usr/bin/env"};
    for (auto const& s : cmd) {
        argv.emplace_back(s.c_str());
    }
    argv.push_back(nullptr);

    auto PATH = []() -> std::string {
        auto ptr = std::getenv("PATH");
        if (!ptr) return "";
        return ptr;
    }();

    if (PATH.empty()) {
        PATH = mountPoint + "/usr/bin";
    } else {
        PATH = mountPoint + "/usr/bin:" + PATH;
    }

    auto _envp = std::vector<std::string>{"PATH=" + PATH,
                                          "SLIX_ROOT=" + mountPoint,
    };
    auto envp = std::vector<char const*>{};
    for (auto& e : _envp) {
        envp.push_back(e.c_str());
    }
    for (auto e = environ; *e != nullptr; ++e) {
        if (std::string_view{*e}.starts_with("PATH=")) continue;
        if (std::string_view{*e}.starts_with("SLIX_ROOT=")) continue;
        envp.push_back(*e);
    }
    envp.push_back(nullptr);

    if (cliVerbose) {
        std::cout << "calling shell";
        for (auto a : argv) {
            if (a == nullptr) continue;
            std::cout << " " << a;
        }
        std::cout << "\n";
    }

    execvpe(argv[0], (char**)argv.data(), (char**)envp.data());
    exit(127);
}
}
