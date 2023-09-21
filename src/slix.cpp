#include <clice/clice.h>
#include <fmt/format.h>

namespace {
void help() {
    fmt::print("{}\n", clice::generateHelp());
    fmt::print("For bash/zsh completion execute `eval \"$(CLICE_GENERATE_COMPLETION=$$ {})\"`\n", clice::argv0);
    exit(0);
}
auto cliHelp = clice::Argument { .args        = {"--help", "-h"},
                                 .desc        = "prints the help page",
                                 .cb          = help,
                                 .cb_priority = 10, // 10 is higher priority than default 100
};
}

int main(int argc, char** argv) {
    try {
        if (auto failed = clice::parse(argc, argv, /*.combineDashes=*/true); failed) {
            fmt::print(stderr, "parsing failed: {}\n", *failed);
            return 1;
        }
    } catch (std::exception const& e) {
        fmt::print(stderr, "error: {}\n", e.what());
    }
    return 0;
}
