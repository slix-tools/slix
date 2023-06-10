#include "GarFuse.h"
#include "MyFuse.h"
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
auto cli = clice::Argument{ .arg    = "shell",
                                   .desc   = "starts a slix shell",
                                   .cb     = app,
};

auto cliCommand = clice::Argument { .parent = &cli,
                                    .arg   = {"-c"},
                                    .desc  = "program to execute inside the shell",
                                    .value = std::vector<std::string>{},
};

auto cliPackages = clice::Argument{ .parent = &cli,
                                    .arg   = "-p",
                                    .desc  = "packages to initiate inside the environment",
                                    .value = std::vector<std::string>{},
};
auto cliMountPoint = clice::Argument{ .parent = &cli,
                                      .arg  = "--mount",
                                      .desc = "path to the mount point or if already in use, reuse",
                                      .value = std::string{},
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

    if (!std::filesystem::exists(std::filesystem::path{mountPoint} / "slix-lock")) {
        auto binary  = std::filesystem::canonical("/proc/self/exe");

        auto call = binary.string();
        if (cliVerbose) call += " --verbose";
        call += " mount --fork --mount " + mountPoint + " -p";
        for (auto p : *cliPackages) {
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

    auto argvStr = std::vector<std::string>{*cliCommand};
    auto argv = std::vector<char const*>{"/usr/bin/env"};
    for (auto const& s : argvStr) {
        argv.emplace_back(s.c_str());
    }
    argv.push_back(nullptr);

    auto _envp = std::vector<std::string>{"PATH=" + mountPoint + "/usr/bin",
                                          "LD_LIBRARY_PATH=" + mountPoint + "/usr/lib",
                                          "SLIX_ROOT=" + mountPoint,
    };
    auto envp = std::vector<char const*>{};
    for (auto& e : _envp) {
        envp.push_back(e.c_str());
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
