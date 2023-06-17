#include "slix-index.h"
#include "PackageIndex.h"

#include <clice/clice.h>
#include <filesystem>

namespace {
void app();
auto cli = clice::Argument{ .parent = &cliIndex,
                            .arg    = "init",
                            .desc   = "initializes a new index",
                            .value  = std::filesystem::path{},
                            .cb     = app,
};

void app() {
    auto index = PackageIndex{};
    //!TODO must check if *cli is existing folder, or create it?
    index.storeFile(*cli / "index.db");
}
}
