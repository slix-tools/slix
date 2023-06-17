#include "slix-index.h"
#include "PackageIndex.h"
#include "GarFuse.h"

#include <clice/clice.h>
#include <filesystem>
#include <fmt/format.h>
#

namespace {
void app();
auto cli = clice::Argument{ .parent = &cliIndex,
                            .arg    = "info",
                            .desc   = "info about a package",
                            .value  = std::filesystem::path{},
                            .cb     = app,
};
auto cliName    = clice::Argument { .parent = &cli,
                                    .arg    = "--name",
                                    .desc   = "name of the package",
                                    .value  = std::string{},
};

auto cliDependencies    = clice::Argument { .parent = &cli,
                                            .arg    = "--dependencies",
                                            .desc   = "list dependencies of latest",
};


void app() {
    if (!exists(*cli / "index.db")) {
        throw std::runtime_error{"path " + (*cli).string() + " does not exists."};
    }

    // Load Index
    auto index = PackageIndex{};
    index.loadFile(*cli / "index.db");


    if (cliName) {
        auto iter = index.packages.find(*cliName);
        if (iter == index.packages.end()) {
            fmt::print("no package with name \"{}\" available\n", *cliName);
            return;
        }

        if (cliDependencies) {
            auto const& info = iter->second.back();
            for (auto d : info.dependencies) {
                fmt::print("{}\n", d);
            }
        } else {
            for (auto const& info : iter->second) {
                fmt::print("{}@{}#{}\n", *cliName, info.version, info.hash);
            }
        }
    } else {
        for (auto const& [name, infos] : index.packages) {
            fmt::print("name: {}\n", name);
            for (auto const& info :infos) {
                fmt::print("  hash: {}\n", info.hash);
                fmt::print("  version: {}\n", info.version);
                fmt::print("  dependencies:\n");
                for (auto const& d : info.dependencies) {
                    fmt::print("    - {}\n", d);
                }
            }
        }
    }
}
}
