// SPDX-FileCopyrightText: 2023 S. G. Gottlieb <info.simon@gottliebtfreitag.de>
// SPDX-License-Identifier: AGPL-3.0-only

#include "App.h"
#include "slix.h"
#include "utils.h"
#include "PackageIndex.h"
#include "Stores.h"

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
auto cli = clice::Argument{ .args   = {"store"},
                            .desc   = "information about stores",
                            .cb     = app,
};

void app() {
    auto app = App {
        .verbose = cliVerbose,
    };
    app.init();

    storeInit();
    auto storePath = getSlixConfigPath() / "stores";

    auto packageNames = std::set<std::string>{};
    for (auto const& e : std::filesystem::directory_iterator{storePath}) {
        auto store = Store{e.path()};
        fmt::print("store at: {}\n", getSlixStatePath() / store.name);
        auto index = store.loadPackageIndex();
        auto const& s = store.config.source;
        fmt::print("  - {} ({}) available packages {}\n", s.url, s.type, index.packages.size());
        for (auto const& [key, list] : index.packages) {
            packageNames.insert(key);
        }
    }
    fmt::print("  total packages {}\n", packageNames.size());
}
}
