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

void app() {
    auto app = App{
        .verbose = cliVerbose,
    };
    app.init();

    auto istPkgs        = app.installedPackages();
    auto indices        = app.loadPackageIndices();

    auto argv     = std::vector<std::string>{};
    auto script   = *cli;

    // reloading environment
    if (script.empty()) {
        if (getEnvironmentFile().empty()) {
            fmt::print("No script given and currently not in an environment to reload - failed\n");
            return;
        }
        script = getEnvironmentFile();
    }
    auto packages = readSlixEnvFile(script);

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
