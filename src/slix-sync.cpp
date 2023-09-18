#include "App.h"
#include "slix.h"
#include "utils.h"
#include "PackageIndex.h"
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
auto cli = clice::Argument{ .args   = {"sync", "-S"},
                            .desc   = "synchronizes a package and its dependencies",
                            .value  = std::vector<std::string>{},
                            .cb     = app,
};

auto cliSearch = clice::Argument{ .parent = &cli,
                                  .args   = {"-s"},
                                  .desc   = "search for packages",
                                  .value  = std::vector<std::string>{},
};

auto cliBrief = clice::Argument{ .parent = &cli,
                                 .args   = {"-b"},
                                 .desc   = "keep output brief"
};

auto cliUpdate = clice::Argument{ .parent = &cli,
                                  .args   = {"--update", "-y"},
                                  .desc   = "Check remotes for updated index.db file",
};

auto cliUpgrade = clice::Argument{ .parent = &cli,
                                   .args   = {"--upgrade", "-u"},
                                   .desc   = "upgrades the currently loaded environment description file",
};
auto cliInputFile = clice::Argument{ .parent = &cli,
                                     .args   = {"-f", "--file"},
                                     .desc   = {"Path to an input file to install from"},
                                     .value  = std::filesystem::path{},
};

auto cliDependencies = clice::Argument{ .parent = &cli,
                                        .args   = {"-d", "--dependencies"},
                                        .desc   = {"print not only the searched packages, but also all its dependencies"},
                                        .value  = std::filesystem::path{},
};


void app() {
    auto app = App {
        .verbose = cliVerbose,
    };
    app.init();

    if (cliUpdate) {
        app.update();
    }

    auto istPkgs        = app.installedPackages();
    auto indices        = app.loadPackageIndices();

    if (cliSearch) {
        for (auto pattern : *cliSearch) {
            auto opt = indices.findLatest(pattern);
            if (!opt) continue;
            auto [path, index, key, info] = *opt;
            fmt::print("{}@{}#{}\n", key, info->version, info->hash);
            if (!cliBrief) {
                fmt::print("    {}\n", info->description);
            } else {
                if (cliDependencies) {
                    for (auto d : info->dependencies) {
                        fmt::print("{}\n", d);
                    }
                }
            }
        }
        return;
    }

    auto envFile = std::filesystem::path{};
    if (cliUpgrade) {
        envFile = weakly_canonical(getEnvironmentFile());
        if (envFile.empty()) {
            fmt::print("No environment to upgrade\n");
            return;
        }
        fmt::print("Trying to upgrade {}\n", envFile);
    }


    auto requiredPkgs    = std::unordered_set<std::string>{};

    for (auto p : *cli) {
        requiredPkgs.merge(indices.findDependencies(p));
    }
    if (cliInputFile) {
        for (auto p : readSlixEnvFile(*cliInputFile)) {
            requiredPkgs.merge(indices.findDependencies(p));
        }
    }

    auto envPackages = std::vector<std::string>{};
    if (cliUpgrade) {
        envPackages = readSlixEnvFile(envFile);
    }

    // No packages where selected, select them all
    if (requiredPkgs.empty() and cliUpgrade) {
        for (auto p : envPackages) {
            auto iter = p.find('@');
            if (iter == std::string::npos) throw error_fmt{"slix is in an invalid state, package {} does not contain any '@' symbol", p};
            requiredPkgs.merge(indices.findDependencies(p.substr(0, iter)));
        }
    }

    // install all packages
    for (auto p : requiredPkgs) {
        if (istPkgs.contains(p + ".gar")) continue; // already installed

        auto config = indices.findPackageConfig(p);

        if (cliVerbose) {
            fmt::print("found {} at {}\n", p, config.path);
        }

        app.installPackage(p, config);
    }

    // upgrade
    if (cliUpgrade) {
        auto newPackages = std::vector<std::string>{};
        bool changed{};
        // Search for newer installed packages
        for (auto p : envPackages) {
            auto [name, info] = indices.findInstalled(p, istPkgs);
            if (name != p) {
                fmt::print("upgrading {} → {}\n", p, name);
                changed = true;
            }
            newPackages.emplace_back(p);
        }
        if (!changed) {
            fmt::print("nothing to upgrade\n");
        } else {
            // Read and update original file, not symlink
            while (is_symlink(envFile)) {
                envFile = read_symlink(envFile);
            }
            auto tmpFile = std::filesystem::path{envFile.string() + ".tmp"};
            copy_file(envFile, tmpFile);
            auto ofs = std::ofstream(tmpFile);
            fmt::print(ofs, "#!/usr/bin/env slix-env\n");
            for (auto const& n : newPackages) {
                fmt::print(ofs, "{}\n", n);
            }
            ofs.close();
            rename(tmpFile, envFile);
            fmt::print("reload environment `exec slix env`");
        }
    }
}
}
