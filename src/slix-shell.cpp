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
auto cliCommand = clice::Argument { .arg   = {"-c"},
                                    .desc  = "program to execute inside the shell",
                                    .value = std::string{"bash"},
};

auto cliVerbose = clice::Argument{ .arg    = {"--verbose"},
                                   .desc   = "detailed description of what is happening",
};

auto cliPackages = clice::Argument{ .arg   = "-p",
                                    .desc  = "packages to initiate inside the environment",
                                    .value = std::vector<std::string>{},
};
}


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
    auto mountPoint = create_temp_dir().string();

    auto binPath = std::filesystem::canonical("/proc/self/exe").parent_path() / "slix-mount";

    auto call = std::string{"nohup " + binPath.string() + " --mount " + mountPoint + " -p"};
    for (auto p : *cliPackages) {
        call += " " + p;
    }
    call += " &";
    std::system(call.c_str());
    std::this_thread::sleep_for(std::chrono::seconds{2});

    auto _prog = std::vector<std::string>{"/usr/bin/env", *cliCommand};
    auto argv = std::vector<char const*>{};
    for (auto& a : _prog) {
        argv.push_back(a.c_str());
    }
    argv.push_back(nullptr);

    auto _envp = std::vector<std::string>{"PATH=" + mountPoint + "/usr/bin",
                                          "LD_LIBRARY_PATH=" + mountPoint + "/usr/lib"};
    auto envp = std::vector<char const*>{};
    for (auto& e : _envp) {
        envp.push_back(e.c_str());
    }
    envp.push_back(nullptr);

    execvpe("/usr/bin/env", (char**)argv.data(), (char**)envp.data());
    exit(127);
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
