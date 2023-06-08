#include <clice/clice.h>
#include <iostream>

namespace {
auto cliHelp = clice::Argument { .arg      = {"--help"},
                                 .desc     = "prints the help page",
                                 .cb       = []{ std::cout << clice::generateHelp(); exit(0); },
};
}

int main(int argc, char** argv) {
    try {
        if (auto failed = clice::parse(argc, argv); failed) {
            std::cerr << "parsing failed: " << *failed << "\n";
            return 1;
        }
        if (auto ptr = std::getenv("CLICE_COMPLETION"); ptr) {
            return 0;
        }
    } catch (std::exception const& e) {
        std::cerr << "error: " << e.what() << "\n";
    }
}
