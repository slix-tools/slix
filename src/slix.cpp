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
        // by-pass everything, if called via slix-script sym link
        if (argc == 2 and std::filesystem::path{argv[0]}.filename() == "slix-script") {
            auto args = std::vector<char const*>{argv[0], "script", argv[1], nullptr};
            if (auto failed = clice::parse(args.size()-1, args.data()); failed) {
                std::cerr << "parsing failed: " << *failed << "\n";
                return 1;
            }
        }

        // by-pass everything, if called via slix-env sym link
        if (argc == 2 and std::filesystem::path{argv[0]}.filename() == "slix-env") {
            auto args = std::vector<char const*>{argv[0], "env", argv[1], nullptr};
            if (auto failed = clice::parse(args.size()-1, args.data()); failed) {
                std::cerr << "parsing failed: " << *failed << "\n";
                return 1;
            }
        }

        if (auto failed = clice::parse(argc, argv, /*.combineDashes=*/true); failed) {
            std::cerr << "parsing failed: " << *failed << "\n";
            return 1;
        }
    } catch (std::exception const& e) {
        std::cerr << "error: " << e.what() << "\n";
    }
}
