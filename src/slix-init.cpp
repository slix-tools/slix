#include "slix.h"
#include "utils.h"
#include "UpstreamConfig.h"

#include <clice/clice.h>
#include <fmt/color.h>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
void app();
auto cli = clice::Argument{ .args   = "init",
                            .desc   = "initializes slix for the local user, by setting up a default upstream",
                            .cb     = app,
};
auto cliReset = clice::Argument { .args = "--reset",
                                  .desc = "removes all already installed packages",
};

void app() {
    auto path_upstreams = getUpstreamsPath();
    if (!exists(path_upstreams) or !cliReset) {
        std::filesystem::remove_all(path_upstreams);
        std::filesystem::create_directories(path_upstreams);
        auto ofs = std::ofstream{path_upstreams / "slix-tools.de.conf"};
        ofs << "path=https://slix-tools.de/packages/\ntype=https\n";
        ofs.close();
        // if reset, also remove all packages
        if (cliReset) {
            auto slixStatePath = getSlixStatePath();
            std::filesystem::remove_all(slixStatePath / "packages");
            std::filesystem::create_directories(slixStatePath / "packages");
        }
    }

    auto ppid       = getppid();
    auto parentPath = std::filesystem::canonical(fmt::format("/proc/{}/exe", ppid));
    auto selfPath   = std::filesystem::canonical(fmt::format("/proc/self/exe"));
    auto slixRoot   = selfPath.parent_path().parent_path().parent_path();
    if (parentPath.filename() == "zsh") {
        fmt::print("add `{}` to your .zshrc\n", fmt::format(fg(fmt::color::cyan), "source {}", canonical(slixRoot/"../activate")));
    } else if (parentPath.filename() == "bash") {
        fmt::print("add `{}` to your .bashrc\n", fmt::format(fg(fmt::color::cyan), "source {}", canonical(slixRoot/"../activate")));
    }
}

}
