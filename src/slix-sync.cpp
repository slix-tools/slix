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

auto cliRemove = clice::Argument{ .parent = &cli,
                                  .args   = {"-r", "--remove"},
                                  .desc   = {"removes packages"},
                                  .value = std::vector<std::string>{},
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

    auto supervisor = PackageSupervisor{};
    if (exists(getSlixConfigPath() / "config.yaml")) {
        supervisor.loadFile(getSlixConfigPath() / "config.yaml");
    }

    // search mode
    if (cliSearch) {
        for (auto pattern : *cliSearch) {
            auto res = indices.findMultiLatest(pattern);
            for (auto [path, index, key, info] : res) {
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
        }
        return;
    }

    // remove mode
    if (cliRemove) {
        // remove explicit mentioned packages
        for (auto pattern : *cliRemove) {
            auto [key, info] = indices.findPackageInfo(pattern);
            supervisor.packages[key].explicitMarked = false;
            fmt::print("removed {}\n", key);
        }

        // remove environments
        if (cliInputFile) {
            bool found{};
            for (auto& [key, info] : supervisor.packages) {
                auto iter = info.environments.find(*cliInputFile);
                if (iter != info.environments.end()) {
                    found = true;
                    info.environments.erase(iter);
                }
            }
            if (found) {
                fmt::print("removed {}\n", *cliInputFile);
            }
        }
        // remove packages that don't have any environments or explicit marks
        std::erase_if(supervisor.packages, [](auto const& v) {
            return !v.second.explicitMarked && v.second.environments.empty();
        });

        supervisor.storeFile(getSlixConfigPath() / "config.yaml");
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
        auto [key, info] = indices.findPackageInfo(p);
        if (!supervisor.packages[key].explicitMarked) {
            fmt::print("adding: {}\n", key);
        }
        supervisor.packages[key].explicitMarked = true;
        requiredPkgs.merge(indices.findDependencies(p));
    }
    if (cliInputFile) {
        for (auto p : readSlixEnvFile(*cliInputFile)) {
            requiredPkgs.merge(indices.findDependencies(p));
            supervisor.packages[p].environments.insert(absolute(*cliInputFile));
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
    supervisor.storeFile(getSlixConfigPath() / "config.yaml");

}
}
