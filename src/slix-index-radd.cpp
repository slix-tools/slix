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
                            .args   = "radd",
                            .desc   = "add a package to the index",
                            .value  = std::string{},
                            .cb     = app,
};
auto cliPackage = clice::Argument { .parent = &cli,
                                    .args   = "--package",
                                    .desc   = "path to the package to add",
                                    .value  = std::filesystem::path{},
};

void app() {
    auto url = *cli;
    auto iter = url.find(':');
    if (iter == std::string::npos) {
        fmt::print("is not a remote address\n");
        return;
    }
    auto lpath = url.substr(iter+1);
    auto lurl = url.substr(0, iter);

    if (!exists(*cliPackage)) {
        throw std::runtime_error{"path " + (*cliPackage).string() + " is not readable."};
    }

    // Fetch index.db
    std::system(fmt::format("scp {}/index.db index.db.new", url).c_str());

    // Load Index
    auto index = PackageIndex{};
    index.loadFile("index.db.new");

    auto availablePackages = std::unordered_set<std::string>{};
    for (auto const& [name, infos] : index.packages) {
        for (auto const& info : infos) {
            availablePackages.emplace(name + "@" + info.version + "#" + info.hash);
        }
    }

    // Load Package
    auto package = GarFuse{*cliPackage, false};

    auto hash = fmt::format("{:02x}", fmt::join(sha256sum(*cliPackage), ""));
    auto fileName = fmt::format("{}@{}#{}.gar", package.name, package.version, hash);

    // Check if all required packages are available
    for (auto d : package.dependencies) {
        if (!availablePackages.contains(d)) {
            throw std::runtime_error{fmt::format("can not add {} because dependency {} is missing", (*cliPackage).string(), d)};
        }
    }
    auto& infos = index.packages[package.name];
    if (!infos.empty()) {
        if (infos.back().hash == hash and infos.back().version == package.version) {
            fmt::print("latest entry matches this entry, no action required: {}@{}#{}\n", package.name, package.version, hash);
            return;
        }
    }
    auto& info = index.packages[package.name].emplace_back();
    info.version      = package.version;
    info.hash         = fmt::format("{}", hash);
    info.description  = package.description;
    info.dependencies = package.dependencies;

    fmt::print("adding {} as {}\n", package.name, fileName);

    // Compress package and move to new location
    std::system(fmt::format("zstd -f -q {0} -o {1}.zst", *cliPackage, fileName).c_str());
    std::system(fmt::format("scp {0}.zst {1}/{0}.zst", fileName, url).c_str());

    // Store index back to filesystem
    index.storeFile("index.db.new");
    std::system(fmt::format("scp index.db.new {}/index.db.new", url).c_str());
    std::system(fmt::format("ssh {0} -x 'mv {1}/index.db.new {1}/index.db'", lurl, lpath).c_str());
    fmt::print("done");
/*
    if (!exists(*cli /  "index.db")) {
        throw std::runtime_error{"path " + (*cli / "index.db").string() + " does not exists."};
    }
    if (!exists(*cliPackage)) {
        throw std::runtime_error{"path " + (*cliPackage).string() + " is not readable."};
    }

    // Load Index
    auto index = PackageIndex{};
    index.loadFile(*cli / "index.db");

    auto availablePackages = std::unordered_set<std::string>{};
    for (auto const& [name, infos] : index.packages) {
        for (auto const& info : infos) {
            availablePackages.emplace(name + "@" + info.version + "#" + info.hash);
        }
    }

    // Load Package
    auto package = GarFuse{*cliPackage, false};

    auto hash = fmt::format("{:02x}", fmt::join(sha256sum(*cliPackage), ""));
    auto fileName = fmt::format("{}@{}#{}.gar", *cliName, *cliVersion, hash);

    // Check if all required packages are available
    for (auto d : package.dependencies) {
        if (!availablePackages.contains(d)) {
            throw std::runtime_error{fmt::format("can not add {} because dependency {} is missing", (*cliPackage).string(), d)};
        }
    }
    auto& infos = index.packages[*cliName];
    if (!infos.empty()) {
        if (infos.back().hash == hash and infos.back().version == *cliVersion) {
            fmt::print("latest entry matches this entry, no action required\n");
            return;
        }
    }
    auto& info = index.packages[*cliName].emplace_back();
    info.hash         = fmt::format("{}", hash);
    info.version      = *cliVersion;
    info.dependencies = package.dependencies;

    fmt::print("adding {} as {}\n", *cliName, fileName);


    // Move package to correct location
    std::filesystem::copy_file(*cliPackage, *cli / fileName, std::filesystem::copy_options::overwrite_existing);
    std::system(fmt::format("zstd -f -q --rm {0} -o {0:}.zst", *cli / fileName).c_str());

    // Store index back to filesystem
    index.storeFile(*cli / "index.db.tmp");
    std::filesystem::rename(*cli / "index.db.tmp", *cli / "index.db");*/
}
}
