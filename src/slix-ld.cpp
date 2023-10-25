// SPDX-FileCopyrightText: 2023 S. G. Gottlieb <info.simon@gottliebtfreitag.de>
// SPDX-License-Identifier: AGPL-3.0-only

#include <filesystem>
#include <iostream>
#include <span>
#include <unistd.h>
#include <vector>

int main(int argc, char** argv) {
    bool dryrun  = std::getenv("SLIX_LD_DRYRUN") != nullptr;
    bool verbose = std::getenv("SLIX_LD_DEBUG") != nullptr;

    auto inputPath = std::filesystem::path{argv[0]};
    auto selfExe   = std::filesystem::canonical("/proc/self/exe");

    auto slixRoot = selfExe.parent_path().parent_path().parent_path();

    if (verbose) {
        std::cerr << "slix root: " << slixRoot << "\n";
    }

    // must be located at 'usr/bin/slix-ld'
    if (selfExe.lexically_relative(slixRoot) != "usr/bin/slix-ld") {
        std::cerr << "slix-ld is not in a valid path\n";
        exit(127);
    }

    // must be an absolute path
    if (slixRoot.empty()) {
        std::cerr << "could not determine location of root\n";
        exit(127);
    }

    if (verbose) {
        std::cerr << "redirecting: " << inputPath << " -> " << selfExe << "\n";
    }

    // check if executable is called via PATH
    auto paths = [&]() -> std::vector<std::filesystem::path> {
        auto path = std::string{};
        if (auto ptr = std::getenv("PATH"); ptr) {
            path = ptr;
        }

        auto paths = std::vector<std::filesystem::path>{};
        if (path.empty()) return paths;
        size_t idx = 0;
        do {
            auto end_idx = path.find(':', idx);
            if (end_idx == std::string::npos) {
                paths.emplace_back(path.substr(idx));
                break;
            }
            paths.emplace_back(path.substr(idx, end_idx-idx));
            idx = end_idx + 1;
        } while(idx < path.size());
        return paths;
    }();

    if (inputPath.is_relative()) {
        inputPath = [&]() {
            // search for absolute path in PATH
            for (auto path : paths) {
                if (exists(path / inputPath)) {
                    return absolute(path / inputPath);
                }
            }
            // convert to an absolute path
            return absolute(inputPath);
        }();
    }

    if (verbose) {
        std::cerr << "inputPath: " << inputPath << " " << inputPath.parent_path() << "\n";
    }

    auto dynamicLoader   = slixRoot / "usr/lib/ld-linux-x86-64.so.2";
    auto ld_library_path = slixRoot / "usr/lib";
    auto argv0           = std::string{};
    auto executableName  = std::string{};
    auto trailingArgs    = std::span<char*>{};

    if (!exists(inputPath)) {
        std::cerr << "file " << inputPath << " does not exists\n";
        exit(127);
    }

    if (!is_symlink(inputPath)) {
        if (argc == 2 and (argv[1] == std::string{"--root"})) {
            std::cout << slixRoot.string() << "\n";
            exit(0);
        }
        if (argc < 3) {
            std::cerr << "direct call to slix-ld requires two arguments `slix-ld argv0 executable`\n";
            exit(127);
        }

        argv0 = argv[1];
        auto path = std::filesystem::path{argv[2]};
        auto parent = path.parent_path().string();
        executableName = (inputPath.parent_path() / parent / (std::string{".slix-ld-"} + path.filename().string())).lexically_normal();

        trailingArgs = {argv+3, argv+argc};

    } else {
        auto target = inputPath;
        // follow symlink until last one is pointing to slix-ld
        while (is_symlink(target)) {
            inputPath = target;
            target = read_symlink(inputPath);
            if (target.is_relative()) {
                target = absolute(inputPath.parent_path() / target);
            }
            if (verbose) {
                std::cerr << "target: " << target << " (" << canonical(target) << ")" << "\n";
            }
        }

        argv0 = inputPath.filename().string();
        executableName = (inputPath.parent_path() / (std::string{".slix-ld-"} + argv0)).lexically_normal();
        trailingArgs = {argv+1, argv+argc};
    }


    auto argv2 = std::vector<char const*>{};
    argv2.push_back(dynamicLoader.c_str());
    argv2.push_back("--argv0");
    argv2.push_back(argv0.c_str());
    argv2.push_back("--library-path");
    argv2.push_back(ld_library_path.c_str());
    argv2.push_back(executableName.c_str());
    for (auto a : trailingArgs) {
        argv2.push_back(a);
    }


    argv2.push_back(nullptr);
    if (verbose) {
        std::cerr << "call: ";
        for (auto a : argv2) {
            if (a == nullptr) continue;
            std::cerr << " " << a;
        }
        std::cerr << "\n";
    }

    if (dryrun) {
        std::cout << "call: ";
        for (auto a : argv2) {
            if (a == nullptr) continue;
            std::cout << " " << a;
        }
        std::cout << "\n";
        return 0;
    }
    return execv(argv2[0], (char**)argv2.data());
}
