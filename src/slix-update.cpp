#include "slix.h"
#include "utils.h"
#include "UpstreamConfig.h"

#include <clice/clice.h>
#include <stdexcept>
#include <fstream>
#include <iostream>

namespace {
void app();
auto cli = clice::Argument{ .arg    = "update",
                            .desc   = "updates the known packages from mirrors/remotes",
                            .cb     = app,
};
void app() {
    auto path = getSlixConfigPath() / "upstreams";
    if (exists(path)) {
        for (auto const& e : std::filesystem::directory_iterator{path}) {
            if (e.path().extension() != ".conf") continue;
            auto config = UpstreamConfig{};
            config.loadFile(e.path());
            if (!config.valid()) throw std::runtime_error{"invalid config " + e.path().string()};

            if (config.type == "file") {
                if (!std::filesystem::exists(config.path)) {
                    throw std::runtime_error{"unknown upstream 'file' path '" + config.path + "'"};
                }

                std::cout << "info " << config.path << " " << path << " " << e.path() << "\n";
                auto source = config.path + "/index.db";
                auto dest   = e.path().string() + ".db";
                std::filesystem::copy_file(source, dest, std::filesystem::copy_options::overwrite_existing);

                std::cout << "read " << (config.path + "/index.db") << "\n";
            } else {
                throw std::runtime_error{"unknown upstream type: " + config.type};
            }
        }
    }
}
}
