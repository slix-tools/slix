#include "App.h"
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
                            .desc        = "starts a special environment",
                            .value       = std::string{},
                            .cb          = app,
};

auto cliStack = clice::Argument{ .parent = &cli,
                                 .args   = "--stack",
                                 .desc   = "Will add paths to PATH instead of overwritting, allows stacking behavior",
};

/*
 * Reads a slix environment
 *  - removes first line (the shebang)
 *  - reads until first line that does not start with '#'
 *  - returns lines without starting '#' and leading white spaces
 */
auto readLines(std::filesystem::path const& script) -> std::vector<std::string> {
    auto results = std::vector<std::string>{};
    auto ifs = std::ifstream{script};
    auto line = std::string{};
    std::getline(ifs, line); // ignore first line
    while (std::getline(ifs, line)) {
        while (!line.empty() && line[0] == ' ') {
            line = line.substr(1);
        }
        while (!line.empty() && line.back() == ' ') {
            line = line.substr(0, line.size()-1);
        }
        if (line.empty()) continue;
        results.push_back(line);
    }
    return results;
}

void app() {
    auto app = App{
        .verbose = cliVerbose,
    };
    app.init();

    auto istPkgs        = app.installedPackages();
    auto indices        = app.loadPackageIndices();

    auto argv     = std::vector<std::string>{};
    auto script   = *cli;
    auto packages = readLines(script);

    argv = scanDefaultCommand(packages, indices, istPkgs, argv);
    argv.insert(argv.begin(), "/usr/bin/env");

    auto mountPoint = create_temp_dir().string();
    auto handle = mountAndWait(clice::argv0, mountPoint, packages, cliVerbose);

    auto PATH = app.getPATH();
    if (PATH.empty() || !cliStack) {
        PATH = mountPoint + "/usr/bin";
    } else {
        PATH = mountPoint + "/usr/bin:" + PATH;
    }

    auto envp = std::map<std::string, std::string> {
        {"PATH", PATH},
        {"SLIX_ROOT", mountPoint},
        {"SLIX_ENVIRONMENT", std::filesystem::absolute(script)}
    };

    execute(argv, envp, /*.verbose=*/cliVerbose, /*.keepEnv=*/true);
    exit(127);

}
}
