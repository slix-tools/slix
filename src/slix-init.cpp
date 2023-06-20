#include "slix.h"
#include "utils.h"
#include "UpstreamConfig.h"

#include <clice/clice.h>
#include <stdexcept>
#include <fstream>
#include <iostream>

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
    if (exists(path_upstreams) and !cliReset) {
        fmt::print("slix seems to be initialized, abort\n");
        return;
    }
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

}
