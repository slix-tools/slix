#include "fsx/Reader.h"
#include "InteractiveProcess.h"

#include <atomic>
#include <csignal>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <random>
#include <ranges>
#include <sstream>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

#define FUSE_USE_VERSION 31
#include <fuse.h>
#include <fuse/fuse_lowlevel.h>

template<class... Ts>
struct overloaded : Ts... { using Ts::operator()...; };

auto create_temp_dir() -> std::filesystem::path {
    auto tmp_dir = std::filesystem::temp_directory_path();
    auto rdev = std::random_device{};
    auto prng = std::mt19937{rdev()};
    auto rand = std::uniform_int_distribution<uint64_t>{0};
    for (size_t i{0}; i < 100; ++i) {
        auto ss = std::stringstream{};
        ss << "slix-fs-" << std::hex << rand(prng);
        auto path = tmp_dir / ss.str();
        // true if the directory was created.
        if (std::filesystem::create_directory(path)) return path;
    }
    throw std::runtime_error{"failed creating temporary directory"};
}

struct GarFuse {
    std::filesystem::path rootPath;

    std::unordered_map<std::string, fsx::Reader::Entry> entries;

    fsx::Reader reader;

    std::vector<std::string> dependencies;

    GarFuse(std::filesystem::path rootPath_)
        : rootPath{std::move(rootPath_)}
        , reader{rootPath}
    {
        std::cout << "opening: " << rootPath << "\n";

        std::string rootfs = "rootfs";
        for (auto entry = reader.readNext(); entry; entry = reader.readNext()) {
            if (entry->name == "dependencies.txt") {
                // special list with dependencies
                auto const& [h, name, offset] = *entry;
                auto buffer = std::string{};
                buffer.resize(h.size);
                auto ct = reader.readContent(buffer.data(), buffer.size(), offset);
                buffer.resize(ct);
                for (auto part : std::views::split(buffer, '\n')) {
//                for (auto part : split(buffer, '\n')) {
                    auto v = std::string_view{&*part.begin(), part.size()};
                    auto s = std::string(v);
                    if (s.empty()) continue;
                    dependencies.push_back(rootPath.parent_path() / s);
                    //std::cout << "found dependency: " << s << "\n";
                }

                //std::cout << "loaded dependencies: " << buffer << "\n";
                continue;
            }
            auto name = entry->name.substr(rootfs.size());
            //std::cout << "adding: " << name << "\n";
            if (name.empty()) name = "/";
            entries.try_emplace(std::move(name), std::move(*entry));
        }
    }
    GarFuse(GarFuse const&) = delete;
    GarFuse(GarFuse&& oth) noexcept = default;

    auto findEntry(std::string const& v) const -> fsx::Reader::Entry const* {
        auto iter = entries.find(v);
        if (iter == entries.end()) return nullptr;
        return &iter->second;
    }

    int getattr_callback(char const* path, struct stat* stbuf) {
        auto entry = findEntry(path);
        //std::cout << "gar - getattr: " << path << " " << (bool)entry << "\n";
        if (!entry) return -ENOENT;
        auto const& [h, name, offset] = *entry;

        stbuf->st_nlink = 0;
        stbuf->st_mode  = [&]() {
            switch(h.type) {
            case 0: return S_IFREG;
            case 1: return S_IFDIR;
            case 2: return S_IFLNK;
            }
            throw std::runtime_error{"unknown type"};
        }() | h.perms;
        stbuf->st_uid  = h.uid;
        stbuf->st_gid  = h.gid;
        stbuf->st_size = h.size;
        return 0;
    }

    int readlink_callback(char const* path, char* targetBuf, size_t size) {
        auto entry = findEntry(path);
 //       std::cout << "readlink: " << path << " " << (bool)entry << "\n";
        if (!entry) return -ENOENT;
        auto const& [h, name, offset] = *entry;
        if (h.type != 2) return -ENOENT;

        size = std::min(entry->header.size, size-1);
        auto ct = reader.readContent(targetBuf, size, offset);
        targetBuf[size] = '\0';
        //std::cout << "read link: " << targetBuf << " " << entry->file_offset << " " << entry->header.size << " " << ct << "\n";
        return 0;
    }
    int open_callback(char const* path, fuse_file_info* fi) {
        auto entry = findEntry(path);
//        std::cout << "open: " << path << " " << (bool)entry << "\n";
        if (!entry) return -ENOENT;
        auto const& [h, name, offset] = *entry;
        if (h.type != 0) return -ENOENT;
        return 0;
    }

    int read_callback(char const* path, char* buf, size_t size, off_t offset_, fuse_file_info* fi) {
        auto entry = findEntry(path);
//        std::cout << "read: " << path << " " << (bool)entry << "\n";
        if (!entry) return -ENOENT;
        auto const& [h, name, offset] = *entry;
        if (h.type != 0) return -ENOENT;

//        std::cout << "reading: " << size << "bytes from " << offset_ << " " << offset << "\n";

        auto ct = reader.readContent(buf, size, offset + offset_);
        return ct;
/*        auto node = tree.findPath(path);
        if (!node) return -ENOENT;

        auto dataread = pread(fi->fh, buf, size, offset);
        std::cout << "read " << path << " " << size << " " << dataread << " " << offset << "\n";
        return dataread;*/
    }
    int readdir_callback(char const* path, void* buf, fuse_fill_dir_t filler, std::unordered_set<std::string>& satisfiedFiles) {
        auto entry = findEntry(path);
        //std::cout << "readdir: " << path << " " << (bool)entry << "\n";
        if (!entry) return -ENOENT;
        auto const& [h, name, offset] = *entry;
        if (h.type != 1) return -ENOENT;

        auto fpath = std::string_view{path};

        //std::cout << "listing: " << path << " " << fpath << " (" << rootPath << "\n";
        std::string_view fpath2 = fpath;
        for (auto const& s : entries) {
            if (s.first.starts_with(fpath) && s.first.size() > fpath.size() + 1) {
                if (s.first[fpath.size()] != '/' and fpath.size() > 1) continue;
                auto pos = s.first.find('/', fpath.size()+1);
//                std::cout << " - skip: " << s.first << " found at " << s.first.substr(s.first.find('/', fpath.size()+1)) << "\n";
                if (pos != std::string_view::npos) continue;
                auto child_name = std::filesystem::path{s.first}.lexically_proximate(fpath).string();
                if (!satisfiedFiles.contains(child_name)) {
                    satisfiedFiles.emplace(child_name);
                    //std::cout << " - child: " << child_name << "\n";
                    filler(buf, child_name.c_str(), nullptr, 0);
                }
            }
        }
        return 0;
/*        auto node = tree.findPath(path);
        if (!node) return -ENOENT;

        for (auto const& [child_name, child_node] : node->children) {
            if (!satisfiedFiles.contains(child_name)) {
                filler(buf, child_name.c_str(), nullptr, 0);
                satisfiedFiles.emplace(child_name);
            }
        }
        std::cout << "trying to read " << path << "\n";
        return 0;*/
    }
};



struct MyFuse {
    bool tearDownMountPoint{}; // remember if mount point had to be created by this class
    fuse_chan* channel {nullptr};
    fuse*      fusePtr {nullptr};
    std::filesystem::path mountPoint;

    std::vector<GarFuse> nodes;

    MyFuse(std::vector<GarFuse> nodes_)
        : mountPoint{create_temp_dir()}
        , nodes{std::move(nodes_)} {
        std::cout << "creating mount point at " << mountPoint << "\n";
        {
            fuse_args args = FUSE_ARGS_INIT(0, nullptr);

            for (auto a : {"", "-oauto_unmount"}) {
                fuse_opt_add_arg(&args, a);
            }
            channel = fuse_mount(mountPoint.c_str(), &args);
            fuse_opt_free_args(&args);
        }
        if (not channel) {
            throw std::runtime_error{"cannot mount"};
        }
        auto foperations = fuse_operations {
            .getattr  = [](char const* path, struct stat* stbuf) { return self().getattr_callback(path, stbuf); },
            .readlink = [](char const* path, char* targetBuf, size_t size) { return self().readlink_callback(path, targetBuf, size); },
            .mknod    = [](char const* path, mode_t m, dev_t d) { return self().mknod_callback(path, m, d); },
            .mkdir    = [](char const* path, mode_t m) { return self().mkdir_callback(path, m); },
            .unlink   = [](char const* path) { return self().unlink_callback(path); },
            .rmdir    = [](char const* path) { return self().rmdir_callback(path); },
            .symlink  = [](char const* path, char const* target) { return self().symlink_callback(path, target); },
            .rename   = [](char const* path, char const* target) { return self().rename_callback(path, target); },
            .chmod    = [](char const* path, mode_t m) { return self().chmod_callback(path, m); },
            .chown    = [](char const* path, uid_t uid, gid_t gid) { return self().chown_callback(path, uid, gid); },
//            .truncate = [](char const* path, off_t offset) -> int { std::cout << "truncate " << path << " " << offset << "\n"; return 0; },
            .open     = [](char const* path, fuse_file_info* fi) { return self().open_callback(path, fi); },
            .read     = [](char const* path, char* buf, size_t size, off_t offset, fuse_file_info* fi) { return self().read_callback(path, buf, size, offset, fi); },
            .write    = [](char const* path, char const* buf, size_t size, off_t offset, fuse_file_info* fi) { return self().write_callback(path, buf, size, offset, fi); },
            .statfs   = [](char const* path, struct statvfs* fs) { return self().statfs_callback(path, fs); },
            .release  = [](char const* path, fuse_file_info* fi) { return self().release_callback(path, fi); },
            .readdir  = [](char const* path, void* buf, fuse_fill_dir_t filler, off_t, fuse_file_info*) { return self().readdir_callback(path, buf, filler); },
            .lock     = [](char const* path, fuse_file_info* fi, int cmd, flock* l) { return self().lock_callback(path, fi, cmd, l); },
            .utimens  = [](char const* path, struct timespec const tv[2]) { return self().utimens_callback(path, tv); }
            //.access   = [](char const* path, int mask) { std::cout << "access: " << path << "\n"; if (auto res = access(path, mask); res == -1) return -errno; return 0; },
//            .read_buf = [](char const* path, fuse_bufvec** bufp, size_t size, off_t offset, fuse_file_info* fi) { return self().read_buf_callback(path, bufp, size, offset, fi); },
//            .lseek    = [](char const* path, off_t off, int whence) -> off_t { std::cout << "lseek not implemented: " << path << "\n"; return 0; },

        };
        fusePtr = fuse_new(channel, nullptr, &foperations, sizeof(foperations), this);
    }

    ~MyFuse() {
        fuse_destroy(fusePtr);

        remove(mountPoint);
    }

    void close() {
        fuse_unmount(mountPoint.c_str(), channel);
    }

    void loop() {
        fuse_loop(fusePtr);
    }


    static auto self() -> MyFuse& {
        auto context = fuse_get_context();
        auto fusefs = reinterpret_cast<MyFuse*>(context->private_data);
        return *fusefs;
    }

    template <typename CB>
    static int any_callback(CB && cb) {
        for (auto& fs : self().nodes) {
            auto r = cb(fs);
            if (r != -ENOENT) return r;
        }
        return -ENOENT;
    }


    #define fwd_callback(name) \
        template <typename ...Args> \
        int name(char const* path, Args&&... args) { \
            auto r = any_callback([&](auto& fs) -> int { \
                if constexpr (requires { fs.name(path, std::forward<Args>(args)...); }) { \
                    return fs.name(path, std::forward<Args>(args)...); \
                } \
                return -ENOENT; \
            }); \
            return r; \
        }
    fwd_callback(getattr_callback)
    fwd_callback(readlink_callback)
    fwd_callback(mknod_callback)
    fwd_callback(mkdir_callback)
    fwd_callback(unlink_callback)
    fwd_callback(rmdir_callback)
    fwd_callback(symlink_callback)
    fwd_callback(rename_callback)
    fwd_callback(chmod_callback)
    fwd_callback(chown_callback)
    fwd_callback(open_callback)
    fwd_callback(read_callback)
    fwd_callback(write_callback)
    fwd_callback(statfs_callback);
    fwd_callback(release_callback)
    int readdir_callback(char const* path, void* buf, fuse_fill_dir_t filler) {
        auto satisfiedFiles = std::unordered_set<std::string>{};
        for (auto& fs : nodes) {
            fs.readdir_callback(path, buf, filler, satisfiedFiles);
        }
//        std::cout << "reporting files:\n";
//        for (auto s : satisfiedFiles) {
//            std::cout << "  " << s << "\n";
//        }
        return 0;
    }
    fwd_callback(lock_callback)
    fwd_callback(utimens_callback)
};

std::atomic_bool finish{false};

int main(int argc, char** argv) {
    if (argc < 1) {
        throw std::runtime_error{"need at least 1 parameters"};
    }
    auto input = std::vector<std::filesystem::path>{};
    auto layers = std::vector<GarFuse>{};
    auto packages = std::vector<std::string>{};
    auto addedPackages = std::unordered_set<std::string>{};
    for (size_t i{1}; i < argc; ++i) {
        packages.emplace_back(argv[i]);
    }
    while (!packages.empty()) {
        auto input = std::filesystem::path{packages.back()};
        packages.pop_back();
        if (addedPackages.contains(input)) continue;
        addedPackages.insert(input);
        std::cout << "layer " << layers.size() << " - ";
        auto const& fuse = layers.emplace_back(input);
        for (auto const& d : fuse.dependencies) {
            packages.push_back(d);
        }
    }

    static auto onExit = std::function<void(int)>{[](int){}};

    std::signal(SIGINT, [](int signal) {
        onExit(signal);
    });

    auto fuseFS = MyFuse{std::move(layers)};
    auto runningFuse = std::jthread{[&]() {
        while (!finish) {
            fuseFS.loop();
        }
    }};
    try {
        auto envp = std::vector<std::string>{};
        envp.push_back("PATH=" + fuseFS.mountPoint.string() + "/usr/bin");
        envp.push_back("LD_LIBRARY_PATH=" + fuseFS.mountPoint.string() + "/usr/lib");

        auto ip = process::InteractiveProcess{std::vector<std::string>{"/usr/bin/bash", "--norc", "--noprofile"}, envp};
        onExit = [&](int signal) {
            ip.sendSignal(signal);
        };
    } catch(std::exception const& e) {
        std::cout << "error: " << e.what() << "\n";
    }
    onExit = [](int) {};
    finish = true;
    fuseFS.close();
    runningFuse.join();
}
