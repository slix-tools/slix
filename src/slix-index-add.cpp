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
                            .args   = "add",
                            .desc   = "add a package to the index",
                            .value  = std::string{},
                            .cb     = app,
};
auto cliPackage = clice::Argument { .parent = &cli,
                                    .args   = "--package",
                                    .desc   = "path to the package to add",
                                    .value  = std::filesystem::path{},
};


auto prepareIndex(PackageIndex& index, std::filesystem::path indexPath, std::filesystem::path packagePath) -> std::string {
    index.loadFile(indexPath);

    // Read all available packages
    auto availablePackages = std::unordered_set<std::string>{};
    for (auto const& [name, infos] : index.packages) {
        for (auto const& info : infos) {
            availablePackages.emplace(name + "@" + info.version + "#" + info.hash);
        }
    }

    // Load Package
    auto package = GarFuse{packagePath, false};

    auto hash = fmt::format("{:02x}", fmt::join(sha256sum(packagePath), ""));
    auto fileName = fmt::format("{}@{}#{}.gar", package.name, package.version, hash);

    // Check if all required packages are available
    for (auto d : package.dependencies) {
        if (!availablePackages.contains(d)) {
            throw std::runtime_error{fmt::format("can not add {} because dependency {} is missing", packagePath, d)};
        }
    }
    auto& infos = index.packages[package.name];
    if (!infos.empty()) {
        if (infos.back().hash == hash and infos.back().version == package.version) {
            fmt::print("latest entry matches this entry, no action required: {}@{}#{}\n", package.name, package.version, hash);
            return {};
        }
    }
    auto& info = index.packages[package.name].emplace_back();
    info.version      = package.version;
    info.hash         = fmt::format("{}", hash);
    info.description  = package.description;
    info.dependencies = package.dependencies;

    fmt::print("adding {} as {}\n", package.name, fileName);
    return fileName;
}

void remote_add(std::string const& url) {
    auto iter = url.find(':');
    auto lpath = url.substr(iter+1);
    auto lurl = url.substr(0, iter);

    // Fetch index.db
    std::system(fmt::format("scp {}/index.db index.db.new", url).c_str());

    // Load Index
    auto index = PackageIndex{};
    auto fileName = prepareIndex(index, "index.db.new", *cliPackage);
    if (fileName.empty()) return;

    // Compress package and move to new location
    std::system(fmt::format("zstd -f -q {0} -o {1}.zst", *cliPackage, fileName).c_str());
    std::system(fmt::format("scp {0}.zst {1}/{0}.zst", fileName, url).c_str());

    // Store index back to filesystem
    index.storeFile("index.db.new");
    std::system(fmt::format("scp index.db.new {}/index.db.new", url).c_str());
    std::system(fmt::format("ssh {0} -x 'mv {1}/index.db.new {1}/index.db'", lurl, lpath).c_str());
    fmt::print("done");
}

void local_add(std::filesystem::path indexPath) {
    auto indexDB = indexPath / "index.db";
    if (!exists(indexDB)) {
        throw std::runtime_error{"path " + indexDB.string() + " does not exists."};
    }

    // Load Index
    auto index = PackageIndex{};
    auto fileName = prepareIndex(index, indexDB, *cliPackage);
    if (fileName.empty()) return;

    // Move package to correct location
    std::filesystem::copy_file(*cliPackage, indexPath / fileName, std::filesystem::copy_options::overwrite_existing);
    std::system(fmt::format("zstd -f -q --rm {0} -o {0:}.zst", indexPath / fileName).c_str());

    // Store index back to filesystem
    index.storeFile(indexDB); // This should be done by writting a temporary and than calling atomic rename(.) !TODO
}

void app() {
    if (!exists(*cliPackage)) {
        throw std::runtime_error{"path " + (*cliPackage).string() + " is not readable."};
    }

    auto url = *cli;
    auto iter = url.find(':');
    if (iter == std::string::npos) {
        fmt::print("local repository {}\n", url);
        local_add(url);
    } else {
        fmt::print("remote repository {}\n", url);
        remote_add(url);
    }
}
}
