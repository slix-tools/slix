// SPDX-FileCopyrightText: 2023 S. G. Gottlieb <info.simon@gottliebtfreitag.de>
// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "GarFuse.h"
#include "PackageIndex.h"

#include <cstdlib>
#include <curl/curl.h>
#include <filesystem>
#include <indicators/cursor_control.hpp>
#include <indicators/progress_bar.hpp>
#include <indicators/block_progress_bar.hpp>
#include <random>
#include <ranges>
#include <string>
#include <string_view>
#include <sys/ioctl.h>
#include <unistd.h>
#include <unordered_set>

inline auto isTerminal(size_t t) {
    return isatty(t);
}

inline auto isStdInTerminal() { return isTerminal(STDIN_FILENO); }
inline auto isStdOutTerminal() { return isTerminal(STDOUT_FILENO); }
inline auto isStdErrTerminal() { return isTerminal(STDERR_FILENO); }

struct CurlDownload {
    CURL* curl{nullptr};
    FILE* file{nullptr};

    using DownloadCB = std::function<void(curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal , curl_off_t ulnow)>;
    static size_t download_progress_callback(void* clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow) {
        auto funcP = static_cast<DownloadCB*>(clientp);
        (*funcP)(dltotal, dlnow, ultotal, ulnow);
        return 0;
    }
    DownloadCB downloadCB;

    template <typename ...Args>
    void setOption(CURLoption option, Args&&... args) {
        if (curl_easy_setopt(curl, option, std::forward<Args>(args)...) != CURLE_OK) {
            throw std::runtime_error{"setting curl option failed " + std::to_string(option)};
        }
    }


    CurlDownload(std::string src, std::filesystem::path dest, DownloadCB cb)
        : curl{curl_easy_init()}
        , file{fopen(dest.c_str(), "w")}
        , downloadCB{cb}
    {
        if (!curl) throw std::runtime_error{"setup of libcurl failed"};
        if (!file) throw std::runtime_error{"file couldn't be opened"};

        setOption(CURLOPT_XFERINFODATA, &cb);
        setOption(CURLOPT_XFERINFOFUNCTION, download_progress_callback);
        setOption(CURLOPT_NOPROGRESS, 0);
        setOption(CURLOPT_URL, src.c_str());
        setOption(CURLOPT_WRITEDATA, file);

    }
    void perform() {
        if (curl_easy_perform(curl) != CURLE_OK) {
            throw std::runtime_error{"download could not be finished"};
        }
    }

    ~CurlDownload() {
        if (!curl) return;

        fclose(file);
        setOption(CURLOPT_NOPROGRESS, 1);
        curl_easy_cleanup(curl);
    }
};

struct Bar {
    indicators::ProgressBar bar;
    size_t progress{0};

    Bar(std::string prefix, std::string postfix)
        : bar {
            indicators::option::BarWidth{50},
            indicators::option::Start{"["},
            indicators::option::Fill{"="},
            indicators::option::Lead{">"},
            indicators::option::Remainder{" "},
            indicators::option::End{"]"},
            indicators::option::PrefixText{prefix},
            indicators::option::PostfixText{postfix},
            indicators::option::ForegroundColor{indicators::Color::green},
            indicators::option::FontStyles{std::vector<indicators::FontStyle>{indicators::FontStyle::bold}},
        }
    {
        if (isStdOutTerminal()) {
             indicators::show_console_cursor(false);
        }
    }
    void setProgress(int p) {
        if (progress == 100) return;
        progress = p;
        if (isStdOutTerminal()) {
            bar.set_progress(progress);
        } else {
            if (progress == 100) {
                bar.set_progress(progress);
            }
        }
    }
    ~Bar() {
        if (isStdOutTerminal()) {
             indicators::show_console_cursor(true);
        }
    }
};

inline auto getPATH(std::string _default = "") -> std::string {
    auto ptr = std::getenv("PATH");
    if (!ptr) return _default;
    return ptr;
}


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

//!TODO curl is not cleanup properly on failure (how does anyone does this without RAII?)
inline void downloadFile(std::string action, std::string postfix, std::string url, std::filesystem::path dest, bool verbose) {
    auto bar = Bar{action, postfix};
    auto cb = CurlDownload::DownloadCB{[&](curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow) {
        if (dltotal == 0) bar.setProgress(0);
        else bar.setProgress(dlnow * 100 / dltotal);
    }};
    auto curl = CurlDownload{url, dest, cb};
    curl.perform();
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


inline auto is_link_to_directory(std::filesystem::path p) {
    if (is_directory(p)) return true;
    if (is_symlink(p)) return is_link_to_directory(read_symlink(p));
    return false;
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

inline auto mountAndWaitCall(std::filesystem::path argv0, std::filesystem::path mountPoint, std::vector<std::string> const& packages, bool verbose, std::vector<std::string> const& mountOptions) -> std::vector<std::string> {
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
//        if (allowOther) {
//            call.push_back("--allow_other");
//        }
        call.push_back("-p");
        for (auto p : packages) {
            call.push_back(p);
        }
        if (!mountOptions.empty()) {
            call.push_back("--options");
            call.push_back("--");
            for (auto const& o : mountOptions) {
                call.push_back(o);
            }
        }
    }
    return call;
}

inline auto mountAndWait(std::filesystem::path argv0, std::filesystem::path mountPoint, std::vector<std::string> const& packages, bool verbose, std::vector<std::string> const& mountOptions) -> std::ifstream {
    auto call = mountAndWaitCall(argv0, mountPoint, packages, verbose, mountOptions);
    if (!call.empty()) {
        auto callStr = fmt::format("{}", fmt::join(call, " "));
        if (verbose) {
            fmt::print("call mount: `{}`\n", callStr);
        }
        auto ret = std::system(callStr.c_str());
        if (WIFSIGNALED(ret) || (WIFEXITED(ret) && WEXITSTATUS(ret) != 0)) {
            throw error_fmt{"error running {}", callStr};
        }
    }
    auto ifs = std::ifstream{};
    while (!ifs.is_open()) {
        std::this_thread::sleep_for(std::chrono::milliseconds{10}); //!TODO can we do this better to wait for slix-mount to finish?
        ifs.open(mountPoint / "slix-lock");
    }
    return ifs;
}

inline auto scanDefaultCommand(std::vector<std::string> packages, std::unordered_map<std::string, std::filesystem::path> const& packagePaths) -> std::vector<std::string> {
    for (auto input : packages) {
        // find name of package
        auto iter = packagePaths.find(input);
        if (iter == packagePaths.end()) {
            throw error_fmt{"can not find any installed package for {}", input};
        }

        // find package location
        auto fuse = GarFuse{iter->second, false};
        if (fuse.defaultCmd.size()) {
            return fuse.defaultCmd;
        }
    }
    throw error_fmt{"no command given"};
}

struct EnvFile {
    std::vector<std::string> allLines;
    std::unordered_set<std::string> packages{};

    void replacePackage(std::string oldPackage, std::string newPackage) {
        if (!packages.contains(oldPackage)) {
            throw error_fmt{"environment file doesn't contain {}", oldPackage};
        }
        if (packages.contains(newPackage)) {
            throw error_fmt{"environment file already contains {}", newPackage};
        }

        packages.erase(oldPackage);
        packages.insert(newPackage);

        for (auto & l : allLines) {
            if (l == oldPackage) {
                l = newPackage;
            }
        }
    }

    friend auto begin(EnvFile const& env) { return env.packages.begin(); }
    friend auto begin(EnvFile& env)       { return env.packages.begin(); }
    friend auto end(EnvFile const& env)   { return env.packages.end();   }
    friend auto end(EnvFile& env)         { return env.packages.end(); }
};

/*
 * Reads a slix environment
 *
 *  - trimes lines
 */
inline auto readSlixEnvFile(std::filesystem::path const& script) -> EnvFile {
    auto ifs = std::ifstream{script};
    auto line = std::string{};
    auto envFile = EnvFile{};
    while (std::getline(ifs, line)) {
        // remove leading white spaces
        while (!line.empty() && line[0] == ' ') {
            line = line.substr(1);
        }
        // remove trailing white spaces
        while (!line.empty() && line.back() == ' ') {
            line = line.substr(0, line.size()-1);
        }
        // add trimed lines
        envFile.allLines.push_back(line);

        // add package line
        if (!line.empty() && line[0] != '#') {
            envFile.packages.insert(line);
        }
    }
    return envFile;
}

/*
 * Saves a slix environment
 */
inline void saveSlixEnvFile(EnvFile const& env, std::filesystem::path const& script) {
    auto ofs = std::ofstream{script};
    for (auto const& l : env.allLines) {
        fmt::print(ofs, "{}\n", l);
    }
}
