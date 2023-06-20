#include <clice/clice.h>
#include <iostream>

namespace {
auto cliHelp = clice::Argument { .args        = {"--help", "-h"},
                                 .desc        = "prints the help page",
                                 .cb          = []{ std::cout << clice::generateHelp(); exit(0); },
                                 .cb_priority = 10, // 10 is higher priority than default 100
};
}

void slix_script_main(std::string script);

int main(int argc, char** argv) {
    // by pass everything, if called via slix-script sym link
    if (argc == 2 and std::filesystem::path{argv[0]}.filename() == "slix-script") {
        clice::argv0 = std::filesystem::path{argv[0]}.filename();
        slix_script_main(argv[1]);
    }
    try {
        if (auto failed = clice::parse(argc, argv); failed) {
            std::cerr << "parsing failed: " << *failed << "\n";
            return 1;
        }
    } catch (std::exception const& e) {
        std::cerr << "error: " << e.what() << "\n";
    }
}
