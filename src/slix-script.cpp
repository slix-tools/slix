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
auto cli = clice::Argument{ .args        = "script",
                            .desc        = "run in script mode",
                            .value       = std::string{},
                            .cb          = app,
};

/*
 * Reads a slix script
 *  - removes first line (the shebang)
 *  - ignores lines starting with two #
 *  - reads until first line that does not start with '#'
 *  - returns lines without starting '#' and leading white spaces
 */
auto readLines(std::filesystem::path const& script) -> std::vector<std::string> {
    auto results = std::vector<std::string>{};
    auto ifs = std::ifstream{script};
    auto line = std::string{};
    std::getline(ifs, line); // ignore first line
    while (std::getline(ifs, line)) {
        if (line.empty()) break;
        if (line[0] != '#') break;
        if (line.size() >= 2 and line[1] == '#') continue;
        line = line.substr(1);
        while (!line.empty() && line[0] == ' ') {
            line = line.substr(1);
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

    auto packages = std::vector<std::string>{};
    auto argv     = std::vector<std::string>{};
    auto script   = *cli;

    auto lines = readLines(script);
    // parse lines
    int mode = 0; // no mode
    for (auto const& l : lines) {
        if (l == "--packages") {
            mode = 1;
            continue;
        }
        if (l == "--run") {
            mode = 2;
            continue;
        }
        if (mode == 1) {
            packages.push_back(l);
        } else if (mode == 2) {
            argv.push_back(l);
        }
    }
    argv = scanDefaultCommand(packages, indices, istPkgs, argv);
    argv.insert(argv.begin(), "/usr/bin/env");
    argv.push_back(script);

    auto mountPoint = create_temp_dir().string();
    mountAndWait(clice::argv0, mountPoint, packages, cliVerbose);

    auto PATH = app.getPATH();
    if (PATH.empty()) {
        PATH = mountPoint + "/usr/bin";
    } else {
        PATH = mountPoint + "/usr/bin:" + PATH;
    }

    auto envp = std::map<std::string, std::string> {
        {"PATH", PATH},
        {"SLIX_ROOT", mountPoint},
    };

    execute(argv, envp, /*.verbose=*/cliVerbose, /*.keepEnv=*/false);
    exit(127);

}
}
