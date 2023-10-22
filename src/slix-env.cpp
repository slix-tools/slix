// SPDX-FileCopyrightText: 2023 S. G. Gottlieb <info.simon@gottliebtfreitag.de>
// SPDX-License-Identifier: AGPL-3.0-only

#include "slix.h"
#include "utils.h"

#include <clice/clice.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <thread>


namespace {
void app();
auto cli = clice::Argument{ .args        = "env",
                            .symlink     = true,
                            .desc        = "starts a special environment",
                            .value       = std::string{},
                            .cb          = app,
};

auto cliStack = clice::Argument{ .parent = &cli,
                                 .args   = "--stack",
                                 .desc   = "Will add paths to PATH instead of overwritting, allows stacking behavior",
};

auto cliMountPoint = clice::Argument{ .parent = &cli,
                                      .args = "--mount",
                                      .desc = "path to the mount point",
                                      .value = std::string{},
};
auto cliAllowOther = clice::Argument{ .parent = &cli,
                                .args = "--allow_other",
                                .desc = "Allows other users to access to the mounted paths",
};

auto escapeString(std::string const& s) {
    auto str = std::string{};
    for (auto c : s) {
        if (c != '\'') {
            str += c;
        } else {
            str += "\\'";
        }
    }
    return str;
}

auto requiresEscape(std::string const& s) {
    auto escape = std::array<bool, 128>{};
    escape.fill(false);
    for (auto c : "\' \t\n\r\"!#@") {
        escape[c] = true;
    }
    for (auto c : s) {
        if (escape[c]) return true;
    }
    return false;
}
auto quoteStringIfRequired(std::string const& s) {
    if (!requiresEscape(s)) return s;
    return fmt::format("'{}'", escapeString(s));
}

void app() {
    auto script   = *cli;

    // reloading environment
    if (script.empty()) {
        if (getEnvironmentFile().empty()) {
            fmt::print(stderr, "No script given and currently not in an environment to reload - failed\n");
            return;
        }
        script = getEnvironmentFile();
    }
    auto envFile = readSlixEnvFile(script);
    auto packages = std::vector<std::string>(envFile.packages.begin(), envFile.packages.end());

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
    auto call = mountAndWaitCall(clice::argv0, mountPoint, packages, false, cliAllowOther);
    while (std::filesystem::is_symlink(call[0]) and std::filesystem::path{call[0]}.filename() != "slix") {
        auto ncall = std::filesystem::read_symlink(call[0]);
        if (std::filesystem::path{ncall}.is_relative()) {
            call[0] = std::filesystem::canonical(std::filesystem::path{call[0]}.parent_path() / ncall);
        }
    }
    for (auto a : call) {
        fmt::print("{} ", quoteStringIfRequired(a));
    } fmt::print("\n");
    fmt::print("while [ ! -e {} ]; do sleep 0.1; done\n", quoteStringIfRequired(mountPoint));
    fmt::print("exec 3<> {}/slix-lock\n", quoteStringIfRequired(mountPoint));


    auto PATH = getPATH();
    if (PATH.empty() || !cliStack) {
        PATH = mountPoint + "/usr/bin";
    } else {
        PATH = mountPoint + "/usr/bin:" + PATH;
    }

    fmt::print("export PATH={}\n", quoteStringIfRequired(PATH));
    fmt::print("export SLIX_ENVIRONMENT={}\n", quoteStringIfRequired(std::filesystem::weakly_canonical(script).string()));
}
}
