#pragma once
#include "GarFuse.h"
#include "PackageIndex.h"

#include <cstdlib>
#include <curl/curl.h>
#include <filesystem>
#include <random>
#include <ranges>
#include <string>
#include <string_view>
#include <unordered_set>

/**
 * Path to the ~/.cache/slix path
 */
inline auto getSlixCachePath() -> std::filesystem::path {
    auto ptr = std::getenv("XDG_CACHE_HOME");
    if (ptr) return ptr + std::string{"/slix"};
    ptr = std::getenv("HOME");
    if (ptr) return ptr + std::string{"/.cache/slix"};
    throw std::runtime_error{"unknown HOME and XDG_CACHE_HOME"};
}


inline auto create_temp_dir() -> std::filesystem::path {
    auto tmp_dir = getSlixCachePath();
    auto rdev = std::random_device{};
    auto prng = std::mt19937{rdev()};
    auto rand = std::uniform_int_distribution<uint64_t>{0};
    for (size_t i{0}; i < 100; ++i) {
        auto ss = std::stringstream{};
        ss << "slix-fs-" << std::hex << rand(prng);
        auto path = tmp_dir / ss.str();
        // true if the directory was created.
        if (std::filesystem::create_directories(path)) return path;
    }
    throw std::runtime_error{"failed creating temporary directory"};
}

/**
 * Path to the ~/.config/slix path
 */
inline auto getSlixConfigPath() -> std::filesystem::path {
    auto ptr = std::getenv("XDG_CONFIG_HOME");
    if (ptr) return ptr + std::string{"/slix"};
    ptr = std::getenv("HOME");
    if (ptr) return ptr + std::string{"/.config/slix"};
    throw std::runtime_error{"unknown HOME and XDG_CONFIG_HOME"};
}

/**
 * Path to the ~/.local/state/slix path
 */
inline auto getSlixStatePath() -> std::filesystem::path {
    auto ptr = std::getenv("XDG_STATE_HOME");
    if (ptr) return ptr + std::string{"/slix"};
    ptr = std::getenv("HOME");
    if (ptr) return ptr + std::string{"/.local/state/slix"};
    throw std::runtime_error{"unknown HOME and XDG_STATE_HOME"};
}

/**
 * Path to the last loaded environment file (or empty if non available)
 */
inline auto getEnvironmentFile() -> std::filesystem::path {
    auto ptr = std::getenv("SLIX_ENVIRONMENT");
    if (ptr) return ptr;
    return "";
}



/**
 * get path to upstream directories
 */
inline auto getUpstreamsPath() -> std::filesystem::path {
    auto path = getSlixConfigPath() / "upstreams";
    if (!exists(path)) {
        throw std::runtime_error{"missing path: " + path.string()};
    }
    return path;
}

/** returns a list of installed packages - including the trailing .gar
 */
inline auto installedPackages(std::vector<std::filesystem::path> const& slixPkgPaths) -> std::unordered_set<std::string> {
    auto results = std::unordered_set<std::string>{};
    for (auto p : slixPkgPaths) {
        for (auto pkg : std::filesystem::directory_iterator{p}) {
            results.insert(pkg.path().filename().string());
        }
    }
    return results;
}

/** searches through paths for a package with exact name
 */
inline auto searchPackagePath(std::vector<std::filesystem::path> const& slixPkgPaths, std::string const& name) -> std::filesystem::path {
    for (auto p : slixPkgPaths) {
        for (auto pkg : std::filesystem::directory_iterator{p}) {
            auto filename = pkg.path().filename();
            if (filename == name) {
                return pkg.path();
            }
        }
    }
    throw std::runtime_error{"couldn't find path for " + name};
}

inline void unpackZstFile(std::filesystem::path file) {
    // extract file
    auto dest = file;
    dest.replace_extension();
    auto call = fmt::format("zstd -q -d --rm {} -o {}", file, dest);
    std::system(call.c_str());
}
inline void downloadFile(std::string url, std::filesystem::path dest, bool verbose) {
    CURL* curl = curl_easy_init();
    if (!curl) throw std::runtime_error{"setup of libcurl failed"};
    auto file = fopen(dest.c_str(), "w");
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, file);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0);
    curl_easy_perform(curl);
    fclose(file);
    if (verbose) {
        fmt::print("downloading {} -> {}\n", url, dest);
    }
    curl_easy_cleanup(curl);
}

//!TODO requires much better url encoding
inline auto encodeURL(std::string input) -> std::string {
    std::string output;
    for (auto c : input) {
        if (c == '#') {
            output += "%23";
        } else {
            output += c;
        }
    }
    return output;
}

/**
 * load all package indices
 */
inline auto loadPackageIndices() -> PackageIndices {
    auto pathUpstreams = getUpstreamsPath();
    auto indices = std::unordered_map<std::filesystem::path, PackageIndex>{};
    for (auto const& e : std::filesystem::directory_iterator{pathUpstreams}) {
        if (e.path().extension() != ".db") continue;
        auto& index = indices[e.path()];
        index.loadFile(e.path());
    }
    return {indices};
}

inline void execute(std::vector<std::string> const& _argv, std::map<std::string, std::string> _envp, bool verbose, bool keepEnv) {
    // set argv variables
    auto argv = std::vector<char const*>{};
    for (auto const& s : _argv) {
        argv.emplace_back(s.c_str());
    }

    auto envp = std::vector<char const*>{};
    for (auto& [key, value] : _envp) {
        value = key + "=" + value;
        envp.push_back(value.c_str());
    }
    if (keepEnv) {
        for (auto e = environ; *e != nullptr; ++e) {
            [&]() {
                for (auto const& [key, value] : _envp) {
                    auto view = std::string_view{*e};
                    if (view.size() > key.size()
                        and view[key.size()] == '=' && view.starts_with(key)
                        and view.starts_with(key)
                    ) {
                        return;
                    }
                }
                envp.push_back(*e);
            }();
        }
    }

    // call exec
    if (verbose) {
        fmt::print("calling `{}`\n", fmt::join(argv, " "));
    }
    argv.push_back(nullptr);
    envp.push_back(nullptr);

    execvpe(argv[0], (char**)argv.data(), (char**)envp.data());
}

inline auto mountAndWaitCall(std::filesystem::path argv0, std::filesystem::path mountPoint, std::vector<std::string> const& packages, bool verbose, bool allowOther) -> std::vector<std::string> {
    auto call = std::vector<std::string>{};
    if (!std::filesystem::exists(std::filesystem::path{mountPoint} / "slix-lock")) {
        if (verbose) {
            fmt::print("argv0: {}\n", argv0);
            fmt::print("self-exe: {}\n", std::filesystem::canonical("/proc/self/exe"));
        }
        auto binary = std::filesystem::canonical("/proc/self/exe").parent_path().parent_path() / "bin" / argv0.filename();
        call.push_back(binary.string());

        if (verbose) {
            call.push_back("--verbose");
        }
        call.push_back("mount");
        call.push_back("--fork");
        call.push_back("--mount");
        call.push_back(mountPoint.string());
        if (allowOther) {
            call.push_back("--allow_other");
        }
        call.push_back("-p");
        for (auto p : packages) {
            call.push_back(p);
        }
    }
    return call;
}

inline auto mountAndWait(std::filesystem::path argv0, std::filesystem::path mountPoint, std::vector<std::string> const& packages, bool verbose, bool allowOther) -> std::ifstream {
    auto call = mountAndWaitCall(argv0, mountPoint, packages, verbose, allowOther);
    if (!call.empty()) {
        auto callStr = fmt::format("{}", fmt::join(call, " "));
        if (verbose) {
            fmt::print("call mount: `{}`\n", callStr);
        }
        std::system(callStr.c_str());
    }
    auto ifs = std::ifstream{};
    while (!ifs.is_open()) {
        std::this_thread::sleep_for(std::chrono::milliseconds{10}); //!TODO can we do this better to wait for slix-mount to finish?
        ifs.open(mountPoint / "slix-lock");
    }
    return ifs;
}

inline auto scanDefaultCommand(std::vector<std::string> packages, PackageIndices& indices, std::unordered_set<std::string> istPkgs, std::vector<std::string> cmd) -> std::vector<std::string> {
    if (cmd.empty()) {
        auto slixPkgPaths = std::vector{getSlixStatePath() / "packages"};
        for (auto input : packages) {
            // find name of package
            auto [fullName, info] = indices.findInstalled(input, istPkgs);
            if (fullName.empty()) {
                throw error_fmt{"can not find any installed package for {}", input};
            }
            // find package location
            auto path = searchPackagePath(slixPkgPaths, fullName + ".gar");
            auto fuse = GarFuse{path, false};
            cmd = fuse.defaultCmd;
            if (!cmd.empty()) break;
        }
    }
    if (cmd.empty()) {
        throw error_fmt{"no command given"};
    }
    return cmd;
}

/*
 * Reads a slix environment
 *  - removes first line (the shebang)
 */
inline auto readSlixEnvFile(std::filesystem::path const& script) -> std::vector<std::string> {
    auto results = std::vector<std::string>{};
    auto ifs = std::ifstream{script};
    auto line = std::string{};
    std::getline(ifs, line); // ignore first line
    while (std::getline(ifs, line)) {
        while (!line.empty() && line[0] == ' ') {
            line = line.substr(1);
        }
        while (!line.empty() && line.back() == ' ') {
            line = line.substr(0, line.size()-1);
        }
        if (line.empty()) continue;
        results.push_back(line);
    }
    return results;
}
