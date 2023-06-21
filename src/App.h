#pragma once

#include "PackageIndex.h"
#include "UpstreamConfig.h"
#include "error_fmt.h"
#include "utils.h"

#include <functional>
#include <map>


struct App {
    bool verbose;

    /*
     * Path where all upstreams are located
     * !TODO this should rather return a list of paths to *.conf, since they could be found in several locations
     */
    auto getUpstreamsPath() const -> std::filesystem::path {
        auto pathUpstreams = getUpstreamsPath();
        if (!exists(pathUpstreams)) {
            throw error_fmt{"missing upstream path {}", pathUpstreams};
        }
        return pathUpstreams;
    }

    /**
     * Visits each upstream path loads the config and calls the cb
     */
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

    /** Calls the 'update' functionality. Load index.db from all remotes
     */
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

    /**
     * load all available(cached) remote indices
     */
    auto loadPackageIndices() -> PackageIndices {
        auto pathUpstreams = getUpstreamsPath();
        auto indices = std::unordered_map<std::filesystem::path, PackageIndex>{};
        for (auto const& e : std::filesystem::directory_iterator{pathUpstreams}) {
            if (e.path().extension() != ".db") continue;
            auto& index = indices[e.path()];
            index.loadFile(e.path());
        }
        return {indices};
    }

    /** returns a list of installed packages - including the trailing .gar
     */
    auto installedPackages() -> std::unordered_set<std::string> {
        auto slixPkgPaths = std::vector{getSlixStatePath() / "packages"};
        auto results = std::unordered_set<std::string>{};
        for (auto p : slixPkgPaths) {
            for (auto pkg : std::filesystem::directory_iterator{p}) {
                results.insert(pkg.path().filename().string());
            }
        }
        return results;
    }

    /** Installs a single package
     */
    void installPackage(std::string package, UpstreamConfig const& config) {
        package += ".gar.zst";
        auto src  = std::filesystem::path{config.path} / package;
        auto dest = getSlixStatePath() / "packages" / package;

        if (config.type == "file") {
            std::filesystem::copy_file(src, dest, std::filesystem::copy_options::overwrite_existing);
        } else if (config.type == "https") {
            downloadFile(encodeURL(src), dest, verbose);
        } else {
            throw error_fmt{"unknown upstream type {}", config.type};
        }
        unpackZstFile(dest);
    }
};
