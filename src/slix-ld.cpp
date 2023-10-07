// SPDX-FileCopyrightText: 2023 S. G. Gottlieb <info.simon@gottliebtfreitag.de>
// SPDX-License-Identifier: AGPL-3.0-only

#include <iostream>
#include <unistd.h>
#include <vector>
#include <filesystem>

int main(int argc, char** argv) {
    auto p = std::filesystem::path{argv[0]};
    p = std::filesystem::canonical("/proc/self/exe").parent_path().parent_path() / "bin" / p.filename();

    bool verbose = std::getenv("SLIX_LD_DEBUG") != nullptr;

    if (verbose) {
        std::cout << "redirecting: " << p << "\n";
        std::cout << std::filesystem::canonical("/proc/self/exe") << "\n";
    }
    if (!is_symlink(p)) {
        std::cout << "not a symlink, you don't want to execute this directly\n";
        exit(127);
    }
    // follow symlink until last one is pointing to slix-ld
    auto target = read_symlink(p);
    if (target.is_relative()) {
        target = absolute(p.parent_path() / target);
    }
    if (verbose) {
        std::cout << "target: " << target << "\n";
    }
    while (is_symlink(target)) {
        p = target;
        target = read_symlink(p);
        if (target.is_relative()) {
            target = absolute(p.parent_path() / target);
        }
        if (verbose) {
            std::cout << "target: " << target << "\n";
        }
    }

    auto slixRoot = target.parent_path().parent_path().parent_path().string();

    if (verbose) {
        std::cout << "slix root: " << slixRoot << "\n";
    }

    auto argv0 = p.filename().string();
    auto newBP = slixRoot + ("/usr/bin/.slix-ld-" + argv0);

    auto dynamicLoader = slixRoot + "/usr/lib/ld-linux-x86-64.so.2";
    auto ld_library_path = slixRoot + "/usr/lib";

    auto argv2 = std::vector<char const*>{};
    argv2.push_back(dynamicLoader.c_str());
    argv2.push_back("--argv0");
    argv2.push_back(argv0.c_str());
    argv2.push_back("--library-path");
    argv2.push_back(ld_library_path.c_str());

    argv2.push_back(newBP.c_str());
    for (size_t i{1}; i < argc; ++i) {
        argv2.push_back(argv[i]);
    }
    argv2.push_back(nullptr);
    if (verbose) {
        std::cout << "should run " << dynamicLoader << "\n";
    }
    for (auto a : argv2) {
        if (a == nullptr) continue;
        if (verbose) {
            std::cout << " - " << a << "\n";
        }
    }
    return execv(argv2[0], (char**)argv2.data());
}
