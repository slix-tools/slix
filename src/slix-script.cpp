#include "slix.h"
#include "utils.h"

#include <clice/clice.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <thread>

void slix_script_main(std::string script);

namespace {
void app();
auto cli = clice::Argument{ .arg    = "script",
                            .desc   = "run in script mode",
                            .value  = std::string{},
                            .cb     = app,
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
    slix_script_main(*cli);
}
}

void slix_script_main(std::string script) {
    auto packages = std::vector<std::string>{};
    auto argvStr  = std::vector<std::string>{};

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
            argvStr.push_back(l);
        }
    }
    auto mountPoint = create_temp_dir().string();

    if (!std::filesystem::exists(std::filesystem::path{mountPoint} / "slix-lock")) {
        if (cliVerbose) {
            std::cout << "argv0: " << clice::argv0 << "\n";
            std::cout << "self-exe: " << std::filesystem::canonical("/proc/self/exe") << "\n";
        }
        auto binary = std::filesystem::path{clice::argv0};
        binary = std::filesystem::canonical("/proc/self/exe").parent_path().parent_path() / "bin" / binary.filename();

        auto call = binary.string();
        if (cliVerbose) call += " --verbose";
        call += " mount --fork --mount " + mountPoint + " -p";
        for (auto p : packages) {
            call += " " + p;
        }
        std::system(call.c_str());
    }
    auto ifs = std::ifstream{};
    while (!ifs.is_open()) {
        std::this_thread::sleep_for(std::chrono::milliseconds{10}); //!TODO can we do this better to wait for slix-mount to finish?
        ifs.open(mountPoint + "/slix-lock");
    }

    auto argv = std::vector<char const*>{"/usr/bin/env"};
    for (auto const& s : argvStr) {
        argv.emplace_back(s.c_str());
    }
    argv.push_back(script.c_str());
    argv.push_back(nullptr);

    auto _envp = std::vector<std::string>{"PATH=" + mountPoint + "/usr/bin",
                                          "SLIX_ROOT=" + mountPoint,
    };
    auto envp = std::vector<char const*>{};
    for (auto& e : _envp) {
        envp.push_back(e.c_str());
    }
    for (auto e = environ; *e != nullptr; ++e) {
        if (std::string_view{*e}.starts_with("PATH=")) continue;
        if (std::string_view{*e}.starts_with("SLIX_ROOT=")) continue;
        envp.push_back(*e);
    }
    envp.push_back(nullptr);

    execvpe(argv[0], (char**)argv.data(), (char**)envp.data());
    exit(127);
}
