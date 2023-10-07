// SPDX-FileCopyrightText: 2023 S. G. Gottlieb <info.simon@gottliebtfreitag.de>
// SPDX-License-Identifier: AGPL-3.0-only

#include "slix-index.h"
#include "PackageIndex.h"
#include "GarFuse.h"
#include "sha256.h"

#include <clice/clice.h>
#include <filesystem>
#include <fmt/format.h>
#include <fmt/std.h>
#

namespace {
void app();
auto cli = clice::Argument{ .parent = &cliIndex,
                            .args    = "squash",
                            .desc   = "squash a package, and only keep newst entry",
                            .value  = std::filesystem::path{},
                            .cb     = app,
};
auto cliName = clice::Argument { .parent = &cli,
                                 .args   = "--name",
                                 .desc   = "name of the package",
                                 .value  = std::string{},
};

void app() {
    if (!exists(*cli /  "index.db")) {
        throw std::runtime_error{"path " + (*cli / "index.db").string() + " does not exists."};
    }

    // Load Index
    auto index = PackageIndex{};
    index.loadFile(*cli / "index.db");

    for (auto& [name, infos] : index.packages) {
        if (name == *cliName) {
            for (size_t i{0}; i+1 < infos.size(); ++i) {
                auto info = infos[i];
                auto file = fmt::format("{}@{}#{}.gar.zst", name, info.version, info.hash);
                fmt::print("removing from index: {}\n", file);
            }
            auto file = [&]() -> std::string {
                if (infos.empty()) return "";
                infos = {infos.back()};
                auto info = infos.back();
                return fmt::format("{}@{}#{}.gar.zst", name, info.version, info.hash);
            }();
            for (auto pkg : std::filesystem::directory_iterator{*cli}) {
                auto filename = pkg.path().filename().string();
                if (filename == file) continue;
                if (filename.starts_with(name + "@")) {
                    fmt::print("removing from file system: {}\n", filename);
                    std::filesystem::remove(pkg.path());
                }
            }
            break;
        }
    }

    // Store index back to filesystem
    index.storeFile(*cli / "index.db"); // This should be done by writting a temporary and than calling atomic rename(.) !TODO
}
}
