#pragma once

#include "UpstreamConfig.h"
#include "error_fmt.h"
#include "utils.h"

#include <functional>


struct App {
    bool verbose;

    auto getUpstreamsPath() const -> std::filesystem::path {
        auto pathUpstreams = getUpstreamsPath();
        if (!exists(pathUpstreams)) {
            throw error_fmt{"missing upstream path {}", pathUpstreams};
        }
        return pathUpstreams;
    }

    void visitUpstreamsConfig(std::function<void(std::filesystem::path path, UpstreamConfig const& config)> cb) {
        auto pathUpstreams = getUpstreamsPath();
        for (auto const& e : std::filesystem::directory_iterator{pathUpstreams}) {
            if (e.path().extension() != ".conf") continue;
            auto config = UpstreamConfig{};
            config.loadFile(e.path());
            if (!config.valid()) throw error_fmt{"invalid config file {}", e.path().string()};

            cb(e.path(), config);
        }
    }

    void update() {
        visitUpstreamsConfig([&](std::filesystem::path path, UpstreamConfig const& config) {
            if (config.type == "ignore") {
            } else if (config.type == "file") {
                if (!std::filesystem::exists(config.path)) throw error_fmt{"unknown upstream file path {}", config.path};

                auto source = config.path + "/index.db";
                auto dest   = path.string() + ".db";
                std::filesystem::copy_file(source, dest, std::filesystem::copy_options::overwrite_existing);
                fmt::print("pulled update from {}", config.path);
            } else if (config.type == "https") {
                auto source = config.path + "/index.db";
                auto dest   = path.string() + ".db";
                auto call = "curl \"" + source + "\" -o \"" + dest + "\"";
                std::system(call.c_str());
                fmt::print("pulled update from {}", config.path);
            } else {
                throw error_fmt{"unknown upstream type '{}'", config.type};
            }
        });
    }
};
