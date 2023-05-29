#include <iostream>
#include <unistd.h>
#include <vector>
#include <filesystem>
//#include <fstream>

int main(int argc, char** argv) {
    auto p = std::filesystem::path{argv[0]};
    if (!is_symlink(p)) {
        std::cout << "not a symlink, you don't want to execute this directly\n";
    }

    // extracting slix-path
    auto symBinP = p.parent_path(); // path to /tmp/slix-fs-xyz/usr/sym-bin
    auto usrP    = symBinP.parent_path(); // path to /tmp/slix-fs-xyz/usr
    auto slixP   = usrP.parent_path(); // path to /tmp/slix-fs-xyz

    auto str = std::vector<char*>{};
    auto newBP = usrP/"bin"/p.filename();

    auto dynamicLoader = slixP / "usr" / "lib" / "ld-linux-x86-64.so.2";

    auto argv2 = std::vector<char*>{};
    argv2.push_back((char*)dynamicLoader.c_str());
    argv2.push_back((char*)newBP.c_str());
    for (size_t i{1}; i < argc; ++i) {
        argv2.push_back(argv[i]);
    }
    argv2.push_back(nullptr);
    std::cout << "should run " << dynamicLoader << "\n";
    for (auto a : argv2) {
        if (a == nullptr) continue;
        std::cout << " - " << a << "\n";
    }
    return execv(dynamicLoader.c_str(), argv2.data());

}
