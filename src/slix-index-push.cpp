// SPDX-FileCopyrightText: 2023 S. G. Gottlieb <info.simon@gottliebtfreitag.de>
// SPDX-License-Identifier: AGPL-3.0-only

#include "slix-index.h"
#include "PackageIndex.h"
#include "GarFuse.h"

#include <clice/clice.h>
#include <filesystem>
#include <fmt/format.h>
#include <fmt/std.h>
#

namespace {
void app();
auto cli = clice::Argument{ .parent = &cliIndex,
                            .args   = "push",
                            .desc   = "push an index to a remote path",
                            .value  = std::filesystem::path{},
                            .cb     = app,
};

auto cliRemote  = clice::Argument { .parent = &cli,
                                    .args   = {"--remote", "-r"},
                                    .desc   = "path to the remote server",
                                    .value  = std::string{},
};


void app() {
    auto const index_path = *cli;
    auto const url        = *cliRemote;
    if (!exists(index_path / "index.db")) {
        throw std::runtime_error{"path " + index_path.string() + " does not exists."};
    }
    auto lpath = std::string{};
    auto lurl = url;
    auto iter = url.find(':');
    if (iter == std::string::npos) {
        lpath="${HOME}/";
    } else {
        lpath = url.substr(iter+1);
        lurl = url.substr(0, iter);
    }

    std::system(fmt::format("scp {}/index.db {}/index.db.new", index_path, url).c_str());
    std::system(fmt::format("rsync -r {}/ {} --exclude=index.db", index_path, url).c_str());
    std::system(fmt::format("ssh {} -x 'mv {}/index.db.new {}/index.db'", lurl, lpath, lpath).c_str());
    fmt::print("done pushing");
}
}
