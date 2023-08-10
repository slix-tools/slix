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
                            .args    = "add",
                            .desc   = "add a package to the index",
                            .value  = std::filesystem::path{},
                            .cb     = app,
};
auto cliPackage = clice::Argument { .parent = &cli,
                                    .args   = "--package",
                                    .desc   = "path to the package to add",
                                    .value  = std::filesystem::path{},
};

auto cliName = clice::Argument { .parent = &cli,
                                 .args   = "--name",
                                 .desc   = "name of the package",
                                 .value  = std::string{},
};

auto cliVersion = clice::Argument { .parent = &cli,
                                    .args   = "--version",
                                    .desc   = "version of package",
                                    .value  = std::string{},
};

auto cliDescription = clice::Argument { .parent = &cli,
                                        .args   = {"--description", "--desc", "-c"},
                                        .desc   = "description of the package",
                                        .value  = std::string{},
};



void app() {
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
            fmt::print("latest entry matches this entry, no action required: {}@{}#{}\n", *cliName, *cliVersion, hash);
            return;
        }
    }
    auto& info = index.packages[*cliName].emplace_back();
    info.version      = *cliVersion;
    info.hash         = fmt::format("{}", hash);
    info.description  = *cliDescription;
    info.dependencies = package.dependencies;

    fmt::print("adding {} as {}\n", *cliName, fileName);


    // Move package to correct location
    std::filesystem::copy_file(*cliPackage, *cli / fileName, std::filesystem::copy_options::overwrite_existing);
    std::system(fmt::format("zstd -f -q --rm {0} -o {0:}.zst", *cli / fileName).c_str());

    // Store index back to filesystem
    index.storeFile(*cli / "index.db"); // This should be done by writting a temporary and than calling atomic rename(.) !TODO
}
}
