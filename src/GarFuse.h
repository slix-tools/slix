#pragma once
#define FUSE_USE_VERSION 31



#include "fsx/Reader.h"

#include <filesystem>
#include <fuse.h>
#include <fuse/fuse_lowlevel.h>
#include <iostream>
#include <ranges>
#include <unordered_map>
#include <unordered_set>

struct GarFuse {
    std::unordered_map<std::string, fsx::Reader::Entry> entries;

    fsx::Reader reader;

    std::vector<std::string> dependencies;

    bool verbose;

    GarFuse(std::filesystem::path pathToPackage, bool _verbose)
        : reader{pathToPackage}
        , verbose{_verbose}
    {
        if (verbose) {
            std::cout << "opening: " << pathToPackage << "\n";
        }

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
                    auto v = std::string_view{&*part.begin(), part.size()};
                    auto s = std::string(v);
                    if (s.empty()) continue;
                    dependencies.push_back(s);
                }
                continue;
            }
            auto name = entry->name.substr(rootfs.size());
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
    }
    int readdir_callback(char const* path, void* buf, fuse_fill_dir_t filler, std::unordered_set<std::string>& satisfiedFiles) {
        auto entry = findEntry(path);
        //std::cout << "readdir: " << path << " " << (bool)entry << "\n";
        if (!entry) return -ENOENT;
        auto const& [h, name, offset] = *entry;
        if (h.type != 1) return -ENOENT;

        auto fpath = std::string_view{path};

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
    }
};




