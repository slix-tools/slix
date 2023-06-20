#include "slix.h"
#include "utils.h"
#include "UpstreamConfig.h"

#include <clice/clice.h>
#include <stdexcept>
#include <fstream>
#include <iostream>

namespace {
void app();
auto cli = clice::Argument{ .args   = "update",
                            .desc   = "updates the known packages from mirrors/remotes",
                            .cb     = app,
};
void app() {
    auto path_upstreams = getUpstreamsPath();
    if (!exists(path_upstreams)) {
        fmt::print("missing upstream path {}\n", path_upstreams);
        exit(1);
    }
    for (auto const& e : std::filesystem::directory_iterator{path_upstreams}) {
        if (e.path().extension() != ".conf") continue;
        auto config = UpstreamConfig{};
        config.loadFile(e.path());
        if (!config.valid()) throw std::runtime_error{"invalid config " + e.path().string()};

        if (config.type == "ignore") {
        } else if (config.type == "file") {
            if (!std::filesystem::exists(config.path)) {
                throw std::runtime_error{"unknown upstream 'file' path '" + config.path + "'"};
            }

            if (cliVerbose) {
                fmt::print("info {} {} {}\n", config.path, path_upstreams, e.path());
            }
            auto source = config.path + "/index.db";
            auto dest   = e.path().string() + ".db";
            std::filesystem::copy_file(source, dest, std::filesystem::copy_options::overwrite_existing);

            if (cliVerbose) {
                fmt::print("read {}/index.db\n", config.path);
            }
        } else if (config.type == "https") {
            auto source = config.path + "/index.db";
            auto dest   = e.path().string() + ".db";
            auto call = "curl \"" + source + "\" -o \"" + dest + "\"";
            if (cliVerbose) {
                fmt::print("call {}\n", call);
            }
            std::system(call.c_str());
        } else {
            throw std::runtime_error{"unknown upstream type: " + config.type};
        }
    }
}
}
