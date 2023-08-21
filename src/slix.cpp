#include <clice/clice.h>
#include <iostream>

namespace {
auto cliHelp = clice::Argument { .args        = {"--help", "-h"},
                                 .desc        = "prints the help page",
                                 .cb          = []{ std::cout << clice::generateHelp(); exit(0); },
                                 .cb_priority = 10, // 10 is higher priority than default 100
};
}

int main(int argc, char** argv) {
    try {
        if (auto failed = clice::parse(argc, argv, /*.combineDashes=*/true); failed) {
            std::cerr << "parsing failed: " << *failed << "\n";
            return 1;
        }
    } catch (std::exception const& e) {
        std::cerr << "error: " << e.what() << "\n";
    }
    return 0;
}
