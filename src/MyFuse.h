#pragma once
#define FUSE_USE_VERSION 31

#include <filesystem>
#include <fuse.h>
#include <fuse/fuse_lowlevel.h>
#include <iostream>
#include <unordered_set>


struct MyFuse {
    bool tearDownMountPoint{}; // remember if mount point had to be created by this class
    fuse_chan* channel {nullptr};
    fuse*      fusePtr {nullptr};
    std::filesystem::path mountPoint;

    std::vector<GarFuse> nodes;
    bool verbose;

    MyFuse(std::vector<GarFuse> nodes_, bool _verbose)
        : mountPoint{create_temp_dir()}
        , nodes{std::move(nodes_)}
        , verbose{_verbose} {
        if (verbose) {
            std::cout << "creating mount point at " << mountPoint << "\n";
        }
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

