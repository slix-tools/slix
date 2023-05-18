#include <filesystem>
#include <vector>
#include <fstream>
#include <iostream>
#include <unordered_set>
#include <memory>
#include <csignal>
#include <atomic>
#include <cstring>
#include <thread>
#include <unordered_map>
#include <variant>

#define FUSE_USE_VERSION 31
#include <fuse.h>
#include <fuse/fuse_lowlevel.h>

#include "fsx/Reader.h"

template<class... Ts>
struct overloaded : Ts... { using Ts::operator()...; };

struct Node {
    std::unordered_map<std::string, Node> children;

    auto findPath(std::filesystem::path::iterator iter, std::filesystem::path::iterator end) const -> Node const* {
        ++iter;
        if (iter == end) return this;
        if (auto child_iter = children.find(*iter); child_iter != children.end()) {
            return child_iter->second.findPath(iter, end);
        }
        return nullptr;
    }
    auto findPath(std::filesystem::path const &path) const -> Node const* {
        return findPath(path.begin(), path.end());
    }

    auto findPath(std::filesystem::path::iterator iter, std::filesystem::path::iterator end) -> Node* {
        ++iter;
        if (iter == end) return this;
        if (auto child_iter = children.find(*iter); child_iter != children.end()) {
            return child_iter->second.findPath(iter, end);
        }
        return nullptr;
    }
    auto findPath(std::filesystem::path const &path) -> Node* {
        return findPath(path.begin(), path.end());
    }

};

auto scanFolder(std::filesystem::path const& _path) -> Node {
    //std::cout << "scanning: " << _path << "\n";
    auto node = Node{};
    for (auto const& dir_entry : std::filesystem::directory_iterator{_path, std::filesystem::directory_options::skip_permission_denied}) {
        auto name = dir_entry.path().filename().string();
        if (name == "." || name == "..") continue;
        if (dir_entry.is_directory()) {
            auto children = scanFolder(_path / name);
            node.children.try_emplace(std::move(name), std::move(children));
        } else if (dir_entry.is_regular_file()) {
            std::cout << "adding file: " << _path / name << "\n";
            node.children.try_emplace(std::move(name));
        }
        //std::cout << dir_entry.path() << '\n';
    }
    return node;
}

struct FileFuse {
    std::filesystem::path rootPath;
    Node tree;
    bool writable{false};

    FileFuse(std::filesystem::path rootPath_, bool writable_)
        : rootPath{std::move(rootPath_)}
        , tree{scanFolder(rootPath)}
        , writable{writable_}
    {}
    FileFuse(FileFuse&&) noexcept = default;

    auto real_path(std::filesystem::path const& path) const -> std::filesystem::path {
        auto realPath = rootPath / path.lexically_proximate("/");
        return realPath;
    }

    int getattr_callback(char const* path, struct stat* stbuf) {
        auto node = tree.findPath(path);
        std::cout << "ff, getattr: " << path << " -> " << real_path(path) << " " << (bool)node << "\n";
        if (!node) return -ENOENT;

        auto r = lstat(real_path(path).c_str(), stbuf);
        return r;
    }

    int readlink_callback(char const* path, char* targetBuf, size_t size) {
        auto node = tree.findPath(path);
        if (!node) return -ENOENT;

        auto realPath = real_path(path);

        if (!std::filesystem::is_symlink(realPath)) return -ENOENT;

        auto linkPath = std::filesystem::read_symlink(realPath).string();
        size = std::min(size-1, linkPath.size());
        std::copy(linkPath.c_str(), linkPath.c_str()+size, targetBuf);
        targetBuf[size] = '\0';
        std::cout << "read link: " << linkPath << " " << targetBuf << "\n";
        return 0;
    }
    int mknod_callback(char const* path, mode_t m, dev_t d) {
        if (!writable) return -ENOENT;
        std::cout << "mknod for " << path << "\n";

        auto path_ = std::filesystem::path{path};
        auto parent = path_.parent_path();

        if (!std::filesystem::is_directory(real_path(parent))) {
            auto dm = m;
            if (dm | S_IRWXO) dm |= S_IXOTH;
            if (dm | S_IRWXG) dm |= S_IXGRP;
            if (dm | S_IRWXU) dm |= S_IXUSR;
            auto r = mkdir_callback(parent.c_str(), dm);
            if (r == -ENOENT) return -ENOENT;
        }
        auto node = tree.findPath(parent);
        node->children.try_emplace(path_.filename().string());
        return mknod(real_path(path).c_str(), m, d);
    }
    int mkdir_callback(char const* path, mode_t m) {
        if (!writable) return -ENOENT;
        std::cout << "mkdir for " << path << "\n";

        auto path_ = std::filesystem::path{path};
        auto parent = path_.parent_path();
        if (!std::filesystem::is_directory(real_path(parent))) {
            auto r = mkdir_callback(parent.c_str(), m);
            if (r == -ENOENT) return -ENOENT;
        }
        auto node = tree.findPath(parent);

        node->children.try_emplace(path_.filename().string());
        std::cout << "creating " << path << " at " << real_path(path) << "\n";
        return mkdir(real_path(path).c_str(), m);
    }
    int symlink_callback(char const* path, char const* target) {
        if (!writable) return -ENOENT;
        std::cout << "symlink for " << path << "\n";

        auto path_ = std::filesystem::path{path};
        auto parent = path_.parent_path();
        auto node = tree.findPath(parent);
        if (!node) return -ENOENT;
        node->children.try_emplace(path_.filename().string());
        return symlink(path, target);
    }
    int rename_callback(char const* path, char const* target) {
        if (!writable) return -ENOENT;
        std::cout << "rename " << path << " -> " << target << "\n";

        auto node = tree.findPath(path);
        if (!node) return -ENOENT;

        auto path_ = std::filesystem::path{path};
        auto target_ = std::filesystem::path{target};

        auto pn = tree.findPath(path_.parent_path());
        if (!pn) return -ENOENT;

        auto tpn = tree.findPath(target_.parent_path()); // missing dirs must be created
        if (!tpn) return -ENOENT;

        auto r = rename(real_path(path).c_str(), real_path(target).c_str());

        std::cout << "real move: " << real_path(path) << " -> " << real_path(target) << "\n";

        std::cout << "parents: " << path_.parent_path() << " -> " << target_.parent_path() << "\n";
        std::cout << "filename: " << path_.filename() << " -> " << target_.filename() << "\n";
        // we lost the original name
        auto en = pn->children.at(path_.filename());
        pn->children.erase(path_.filename());
        tpn->children[target_.filename()] = std::move(en);
        return r;
    }
    int chmod_callback(char const* path, mode_t m) {
        if (!writable) return -ENOENT;
        auto node = tree.findPath(path);
        if (!node) return -ENOENT;
        return chmod(real_path(path).c_str(), m);
    }
    int chown_callback(char const* path, uid_t uid, gid_t gid) {
        if (!writable) return -ENOENT;
        auto node = tree.findPath(path);
        if (!node) return -ENOENT;
        return chown(path, uid, gid);
    }

    int open_callback(char const* path, fuse_file_info* fi) {
        auto node = tree.findPath(path);
        if (!node) return -ENOENT;

        if (!writable) {
            if (fi->flags & O_WRONLY || fi->flags & O_RDWR) {
                return -ENOENT;
            }
        }

        auto fd = open(real_path(path).c_str(), fi->flags);
        fi->fh = fd;
        std::cout << "open " << path << "\n";
        return 0;
    }

    int read_callback(char const* path, char* buf, size_t size, off_t offset, fuse_file_info* fi) {
        auto node = tree.findPath(path);
        if (!node) return -ENOENT;

        auto dataread = pread(fi->fh, buf, size, offset);
        std::cout << "read " << path << " " << size << " " << dataread << " " << offset << "\n";
        return dataread;
    }
    int write_callback(char const* path, char const* buf, size_t size, off_t offset, fuse_file_info* fi) {
        std::cout << "trying to write\n";
        if (!writable) {
            return -ENOENT;
        }
        return pwrite(fi->fh, buf, size, offset);
    }
    int release_callback(char const* path, fuse_file_info* fi) {
        auto node = tree.findPath(path);
        if (!node) return -ENOENT;

        close(fi->fh);
        return 0;
    }

    int readdir_callback(char const* path, void* buf, fuse_fill_dir_t filler, std::unordered_set<std::string>& satisfiedFiles) {
        auto node = tree.findPath(path);
        if (!node) return -ENOENT;

        std::cout << "listing " << path << "\n";
        for (auto const& [child_name, child_node] : node->children) {
            if (!satisfiedFiles.contains(child_name)) {
                std::cout << " - child: " << child_name << "\n";

                filler(buf, child_name.c_str(), nullptr, 0);
                satisfiedFiles.emplace(child_name);
            }
        }
        std::cout << "trying to read " << path << "\n";
        return 0;
    }
    int utimens_callback(char const* path, struct timespec const tv[2]) {
        std::cout << "trying to set time for: " << path << "\n";
        if (!writable) return -ENOENT;
        auto node = tree.findPath(path);
        if (!node) return -ENOENT;
        return utimensat(AT_FDCWD, real_path(path).c_str(), tv, 0);
    }
};

struct GarFuse {
    std::filesystem::path rootPath;

    std::unordered_map<std::string, fsx::Reader::Entry> entries;

    fsx::Reader reader;

    GarFuse(std::filesystem::path rootPath_)
        : rootPath{std::move(rootPath_)}
        , reader{rootPath}
    {
        std::cout << "opening: " << rootPath << "\n";

        std::string rootfs = "rootfs";
        for (auto entry = reader.readNext(); entry; entry = reader.readNext()) {
            auto name = entry->name.substr(rootfs.size());
            std::cout << "adding: " << name << "\n";
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
        std::cout << "readlink: " << path << " " << (bool)entry << "\n";
        if (!entry) return -ENOENT;
        auto const& [h, name, offset] = *entry;
        if (h.type != 2) return -ENOENT;

        size = std::min(entry->header.size, size-1);
        auto ct = reader.readContent(targetBuf, size, offset);
        targetBuf[size] = '\0';
        std::cout << "read link: " << targetBuf << " " << entry->file_offset << " " << entry->header.size << " " << ct << "\n";
        return 0;
    }
    int open_callback(char const* path, fuse_file_info* fi) {
        auto entry = findEntry(path);
        std::cout << "open: " << path << " " << (bool)entry << "\n";
        if (!entry) return -ENOENT;
        auto const& [h, name, offset] = *entry;
        if (h.type != 0) return -ENOENT;
        return 0;
    }

    int read_callback(char const* path, char* buf, size_t size, off_t offset_, fuse_file_info* fi) {
        auto entry = findEntry(path);
        std::cout << "read: " << path << " " << (bool)entry << "\n";
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

        //std::cout << "listing: " << path << " " << fpath << "\n";
        for (auto const& s : entries) {
            if (s.first.starts_with(fpath) && s.first.size() > fpath.size() + 1) {
                auto pos = s.first.find('/', fpath.size()+1);
//                std::cout << " - skip: " << s.first << " found at " << s.first.substr(s.first.find('/', fpath.size()+1)) << "\n";
                if (pos != std::string_view::npos) continue;
                auto child_name = std::filesystem::path{s.first}.lexically_proximate(fpath).string();
                if (!satisfiedFiles.contains(child_name)) {
                    satisfiedFiles.emplace(child_name);
          //          std::cout << " - child: " << child_name << "\n";
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

    std::vector<std::variant<FileFuse, GarFuse>> nodes;

    MyFuse(std::filesystem::path mountPoint_, std::vector<std::variant<FileFuse, GarFuse>> nodes_, std::vector<std::string> extraOptions)
        : mountPoint{std::move(mountPoint_)}
        , nodes{std::move(nodes_)} {
        if (not is_directory(mountPoint)) {
            create_directory(mountPoint); // try to create the mountpoint
            tearDownMountPoint = true;
        }
        {
            fuse_args args = FUSE_ARGS_INIT(0, nullptr);

            for (auto a : {"", "-oauto_unmount"}) {
                fuse_opt_add_arg(&args, a);
            }
            for (auto o : extraOptions) {
                fuse_opt_add_arg(&args, o.c_str());
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
            .symlink  = [](char const* path, char const* target) { return self().symlink_callback(path, target); },
            .rename   = [](char const* path, char const* target) { return self().rename_callback(path, target); },
            .chmod    = [](char const* path, mode_t m) { return self().chmod_callback(path, m); },
            .chown    = [](char const* path, uid_t uid, gid_t gid) { return self().chown_callback(path, uid, gid); },
            .truncate = [](char const* path, off_t offset) -> int { std::cout << "truncate " << path << " " << offset << "\n"; return 0; },
            .open     = [](char const* path, fuse_file_info* fi) { return self().open_callback(path, fi); },
            .read     = [](char const* path, char* buf, size_t size, off_t offset, fuse_file_info* fi) { return self().read_callback(path, buf, size, offset, fi); },
            .write    = [](char const* path, char const* buf, size_t size, off_t offset, fuse_file_info* fi) { return self().write_callback(path, buf, size, offset, fi); },
            .release  = [](char const* path, fuse_file_info* fi) { return self().release_callback(path, fi); },
            .readdir  = [](char const* path, void* buf, fuse_fill_dir_t filler, off_t, fuse_file_info*) { return self().readdir_callback(path, buf, filler); },
            .utimens  = [](char const* path, struct timespec const tv[2]) { return self().utimens_callback(path, tv); }
            //.access   = [](char const* path, int mask) { std::cout << "access: " << path << "\n"; if (auto res = access(path, mask); res == -1) return -errno; return 0; },
//            .read_buf = [](char const* path, fuse_bufvec** bufp, size_t size, off_t offset, fuse_file_info* fi) { return self().read_buf_callback(path, bufp, size, offset, fi); },
//            .lseek    = [](char const* path, off_t off, int whence) -> off_t { std::cout << "lseek not implemented: " << path << "\n"; return 0; },

        };
        fusePtr = fuse_new(channel, nullptr, &foperations, sizeof(foperations), this);
    }

    ~MyFuse() {
        fuse_unmount(mountPoint.c_str(), channel);
        fuse_destroy(fusePtr);

        if (tearDownMountPoint) {
            remove(mountPoint);
        }
    }

    void loop() {
        auto cmd = fuse_read_cmd(fusePtr);
        if (cmd) {
            fuse_process_cmd(fusePtr, cmd);
        }
    }


    static auto self() -> MyFuse& {
        auto context = fuse_get_context();
        auto fusefs = reinterpret_cast<MyFuse*>(context->private_data);
        return *fusefs;
    }

    template <typename CB>
    static int any_callback(CB && cb) {
        for (auto& fs : self().nodes) {
            auto r = std::visit([&](auto& fs) {
                return cb(fs);
            }, fs);
            if (r != -ENOENT) return r;
        }
        return -ENOENT;
    }


    #define fwd_callback(name) \
        template <typename ...Args> \
        int name(Args&&... args) { \
            return any_callback([&](auto& fs) -> int { \
                if constexpr (requires { fs.name(std::forward<Args>(args)...); }) { \
                    return fs.name(std::forward<Args>(args)...); \
                } \
                return -ENOENT; \
            }); \
        }
    fwd_callback(getattr_callback)
    fwd_callback(readlink_callback)
    fwd_callback(mknod_callback)
    fwd_callback(mkdir_callback)
    fwd_callback(symlink_callback)
    fwd_callback(rename_callback)
    fwd_callback(chmod_callback)
    fwd_callback(chown_callback)
    fwd_callback(open_callback)
    fwd_callback(read_callback)
    fwd_callback(write_callback)
    fwd_callback(release_callback)
    int readdir_callback(char const* path, void* buf, fuse_fill_dir_t filler) {
        auto satisfiedFiles = std::unordered_set<std::string>{};
        for (auto& fs : nodes) {
            std::visit([&](auto& fs) {
                fs.readdir_callback(path, buf, filler, satisfiedFiles);
            }, fs);
        }
        std::cout << "reporting files:\n";
        for (auto s : satisfiedFiles) {
            std::cout << "  " << s << "\n";
        }
        return 0;
    }
    fwd_callback(utimens_callback)
};

std::atomic_bool finish{false};

static std::string gTarget;
static std::jthread gThread;

int main(int argc, char** argv) {
    if (argc < 2) {
        throw std::runtime_error{"need at least 2 parameters"};
    }
    auto input = std::vector<std::filesystem::path>{};
    auto target = std::filesystem::path{};
    gTarget = target = argv[argc-1];
/*    auto layers = Nodes{};
    for (size_t i{1}; i < argc-1; ++i) {
        input.emplace_back(argv[i]);
        std::cout << "layer " << layers.nodes.size() << "\n";
        if (input.back().extension() == ".tar") {
            layers.nodes.emplace_back(input.back(), scanTar(input.back(), -1));
        } else {
            layers.nodes.emplace_back(input.back(), scanFolder(input.back(), -1));
        }
    }*/
    auto layers = std::vector<std::variant<FileFuse, GarFuse>>{};
    for (size_t i{1}; i < argc-1; ++i) {
        input.emplace_back(argv[i]);
        std::cout << "layer " << layers.size() << "\n";
        if (input.back().extension() == ".gar") {
            layers.emplace_back<GarFuse>(input.back());
        } else {
            bool writable = (i == 1);
            layers.emplace_back(FileFuse{input.back(), writable});
        }
    }

    std::signal(SIGINT, [](int signal) {
        if (finish) {
            std::terminate();
        }
        finish = true;
        gThread = std::jthread{[]() {
            try {
                (void)std::filesystem::is_empty(gTarget);
            } catch(...){}
        }};
    });

    auto extraOptions = std::vector<std::string>{};
    if (auto opt = std::getenv("MNTOPT"); opt) {
        auto v = std::string_view{opt};
        auto p = v.find(' ');
        while (p != std::string_view::npos) {
            extraOptions.emplace_back(v.substr(0, p));
            std::cout << "extraOption: " << extraOptions.back() << "\n";
            v = v.substr(p+1);
            p = v.find(' ');
        }
        if (!v.empty()) {
            extraOptions.emplace_back(v);
            std::cout << "extraOption: " << extraOptions.back() << "\n";
        }
    }

    auto fuseFS = MyFuse{target, std::move(layers), extraOptions};
    while (!finish) {
        fuseFS.loop();
    }
    gThread.join();
}
