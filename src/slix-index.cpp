#include "GarFuse.h"
#include "slix-index.h"
#include "slix.h"
#include "utils.h"
#include "PackageIndex.h"

#include <clice/clice.h>
#include <filesystem>
#include <functional>
#include <iostream>
#include <set>
#include <unordered_map>

namespace {
void app();
auto cli = clice::Argument{ .arg    = "index_old",
                            .desc   = "creates an index file over gar files",
                            .value  = std::string{},
                            .cb     = app,
};

void app() {
    auto path = std::filesystem::path{*cli};
    if (!exists(path)) {
        throw std::runtime_error{"path " + path.string() + " does not exists."};
    }
    auto packages = std::unordered_map<std::string, GarFuse>{};
    for (auto const& e : std::filesystem::directory_iterator{path}) {
        if (e.path().extension() == ".gar") {
            packages.try_emplace(e.path().filename(), e.path(), false);
//            std::cout << "found: " << e.path() << "\n";
        }
    }

    // list every package and its dependencies (include recursive)
    auto index = PackageIndex{};
    for (auto const& [key, p] : packages) {
        auto deps = std::set<std::string>{};
        auto stack = std::vector<std::string>{key};
        while (stack.size()) {
            auto n = stack.back();
            stack.pop_back();
            if (deps.contains(n)) continue;
            deps.insert(n);
            for (auto const& d : packages.at(n).dependencies) {
                stack.push_back(d);
            }
        }
        auto& pkg = index.packages[key];
        for (auto const& d : deps) {
//            pkg.dependencies.push_back(d);
        }
    }
    index.storeFile(path/"index.db");
}
}
