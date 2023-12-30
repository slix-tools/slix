// SPDX-FileCopyrightText: 2023 S. G. Gottlieb <info.simon@gottliebtfreitag.de>
// SPDX-License-Identifier: AGPL-3.0-only

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
    storeInit();
    auto storePath = getSlixConfigPath() / "stores";

    auto packageNames = std::set<std::string>{};
    for (auto const& e : std::filesystem::directory_iterator{storePath}) {
        auto store = Store{e.path()};
        auto index = store.loadPackageIndex();
        auto const& s = store.config.source;

        size_t packageCt{};
        size_t uniqPackageCt{};
        size_t packageCtInstalled{};
        size_t uniqPackageCtInstalled{};

        for (auto const& [name, infos] : index.packages) {
            packageCt += infos.size();
            uniqPackageCt += 1;

            bool added = false;
            for (auto const& info : infos) {
                if (store.state.isInstalled(fmt::format("{}@{}#{}", name, info.version, info.hash))) {
                    packageCtInstalled += 1;
                    added = true;
                }
            }
            if (added) {
                uniqPackageCtInstalled += 1;
            }
        }
        fmt::print("  - name: {}\n", store.name);
        fmt::print("    path: {}\n", getSlixStatePath() / store.name);
        fmt::print("    url: {}\n", s.url);
        fmt::print("    url_type: {}\n", s.type);
        fmt::print("    available_packages: {}\n", packageCt);
        fmt::print("    uniq_available_packages: {}\n", uniqPackageCt);
        fmt::print("    installed_available_packages: {}\n", packageCtInstalled);
        fmt::print("    installed_uniq_available_packages: {}\n", uniqPackageCtInstalled);
    }
}
}
