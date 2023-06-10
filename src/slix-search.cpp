#include "slix.h"
#include "utils.h"

#include <clice/clice.h>
#include <filesystem>
#include <iostream>
#include <set>

namespace {
void app();
auto cli = clice::Argument{ .arg    = "search",
                                   .desc   = "search localy for available slix packages",
                                   .value  = std::string{},
                                   .cb     = app,
};

auto searchPackagePath(std::vector<std::filesystem::path> const& slixPkgPaths, std::string const& name) -> std::set<std::filesystem::path> {
    auto results = std::set<std::filesystem::path>{};
    for (auto p : slixPkgPaths) {
        for (auto pkg : std::filesystem::directory_iterator{p}) {
            auto filename = pkg.path().filename();
            if (filename.string().starts_with(name)) {
                results.insert(filename);
            }
        }
    }
    return results;
}

void app() {
    auto slixPkgPaths = getSlixPkgPaths();
    auto results = searchPackagePath(slixPkgPaths, *cli);
    for (auto const& r : results) {
        std::cout << r.string() << "\n";
    }
    exit(0);
}
}
