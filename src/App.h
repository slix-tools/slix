#pragma once

#include "PackageIndex.h"
#include "UpstreamConfig.h"
#include "error_fmt.h"
#include "utils.h"

#include <cstdlib>
#include <fmt/color.h>
#include <functional>
#include <map>

struct App {
    bool verbose;

    void init(bool reset = false) {
        //!TODO There is something of with the definition of ::getUpstreamsPath, maybe it shouldn't throw?
        auto path_upstreams = [&]() {
            try {
                return ::getUpstreamsPath();
            } catch (...) {
                return getSlixConfigPath() / "upstreams";
            }
        }();
        if (exists(path_upstreams) and !reset) {
            return;
        }
        if (!exists(path_upstreams) or reset) {
            std::filesystem::remove_all(path_upstreams);

            UpstreamConfig{}.storeFile("slix-tools.de/config.yaml");

            // if reset, also remove all packages
            if (reset) {
                auto slixStatePath = getSlixStatePath();
                std::filesystem::remove_all(slixStatePath / "packages");
                std::filesystem::create_directories(slixStatePath / "packages");
                std::filesystem::remove_all(getSlixCachePath());
            }
        }

        auto ppid       = getppid();
        auto parentPath = std::filesystem::canonical(fmt::format("/proc/{}/exe", ppid));
        auto selfPath   = std::filesystem::canonical(fmt::format("/proc/self/exe"));
        auto slixRoot   = selfPath.parent_path().parent_path().parent_path();
        try {
            if (parentPath.filename() == "zsh") {
                fmt::print("add `{}` to your .zshrc\n", fmt::format(fg(fmt::color::cyan), "source {}", canonical(slixRoot/"../activate")));
            } else if (parentPath.filename() == "bash") {
                fmt::print("add `{}` to your .bashrc\n", fmt::format(fg(fmt::color::cyan), "source {}", canonical(slixRoot/"../activate")));
            }
        } catch (...) {}
    }

    /*
     * Path where all upstreams are located
     * !TODO this should rather return a list of paths to *.conf, since they could be found in several locations
     */
    auto getUpstreamsPath() const -> std::filesystem::path {
        auto pathUpstreams = ::getUpstreamsPath();
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
            if (!is_link_to_directory(e.path())) continue;
            if (!exists(e.path() / "config.yaml")) {
                throw error_fmt{"upstream folder {} is missing config.yaml file", e.path()};
            }
            auto config = UpstreamConfig{};
            config.loadFile(e.path() / "config.yaml");

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
                auto dest   = path / "index.db";
                std::filesystem::copy_file(source, dest, std::filesystem::copy_options::overwrite_existing);
                fmt::print("pulled update from {}\n", config.path);
            } else if (config.type == "https") {
                auto source = config.path + "/index.db";
                auto dest   = path / "index.db";
                downloadFile(source, dest, false);
                fmt::print("pulled update from {}\n", config.path);
            } else {
                throw error_fmt{"unknown upstream type '{}'", config.type};
            }
        });
    }

    /**
     * load all available(cached) remote indices
     */
    auto loadPackageIndices() -> PackageIndices {
        return ::loadPackageIndices();
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

    auto getPATH() const -> std::string {
        auto ptr = std::getenv("PATH");
        if (!ptr) return "";
        return ptr;
    }
};
