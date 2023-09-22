#include "App.h"
#include "slix.h"
#include "utils.h"
#include "PackageIndex.h"
#include "PackageSupervisor.h"
#include "UpstreamConfig.h"

#include <clice/clice.h>
#include <stdexcept>
#include <fstream>
#include <iostream>
#include <fmt/format.h>
#include <fmt/std.h>
#include <map>
#include <set>

namespace {
void app();
auto cli = clice::Argument{ .args   = {"upstream"},
                            .desc   = "information about upstream connections",
                            .cb     = app,
};

auto cliAdd = clice::Argument{ .args   = {"add"},
                               .desc   = "add a upstream config",
                               .value  = std::filesystem::path{},
};


void app() {
    auto app = App {
        .verbose = cliVerbose,
    };
    app.init();

    if (cliAdd) {
        if (!exists(*cliAdd)) {
            throw error_fmt{"upstream path {} is not valid", *cliAdd};
        }
        if (cliAdd->filename() != "config.yaml") {
            throw error_fmt{"must be a path to a config.yaml"};
        }

        auto name = cliAdd->parent_path().filename();
        if (exists(getSlixConfigPath() / "upstreams" / name)) {
            throw error_fmt{"upstream \"{}\" already registered", name};
        }

        create_symlink(cliAdd->parent_path(), getSlixConfigPath() / "upstreams" / name);
        fmt::print("created symlink for {}\n    {} -> {}\n", name, getSlixConfigPath() / "upstreams" / name, cliAdd->parent_path());
        return;
    }

    fmt::print("upstreams:\n");
    app.visitUpstreamsConfig([&](std::filesystem::path path, UpstreamConfig const& config) {
        auto index = PackageIndex{};
        index.loadFile(path / "index.db");

        fmt::print("  - {}, type: {}, shared: {}, available packages: {}\n", path.filename(), config.type, config.shared, index.packages.size());

    });

}
}
