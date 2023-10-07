// SPDX-FileCopyrightText: 2023 S. G. Gottlieb <info.simon@gottliebtfreitag.de>
// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include "FileHeader.h"
#include "EntryHeader.h"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <span>
#include <stdexcept>
#include <sys/stat.h>
#include <vector>

namespace fsx {

struct Writer {
    std::ofstream ofs;

    Writer(std::filesystem::path path_)
        : ofs{path_, std::ios::binary}
    {
        auto defaultFileHeader = fsx::FileHeader{};
        addPOD(defaultFileHeader);
    }

    void close() {
        ofs.close();
    }

    void addPathAs(std::filesystem::path path, std::filesystem::path newName) {
        auto newNameAsStr = newName.string();
        struct stat stat_{};
        auto r = lstat(path.c_str(), &stat_);
        if (r != 0) {
            throw std::runtime_error{"error calling lstat"};
        }
        auto type = [&]() -> uint8_t {
            switch (stat_.st_mode & S_IFMT) {
            case S_IFREG: return 0; // file
            case S_IFDIR: return 1; // directory
            case S_IFLNK: return 2; // link
            }
            return 255; // unknown type
        }();
        if (type == 255) {
            std::cerr << "unsupported type for " << path << " - skipping " << (int) stat_.st_mode << "\n";
        }
        auto perms = [&]() -> uint16_t {
            return (stat_.st_mode & ~S_IFMT);
        }();
        auto size = [&]() -> uint64_t {
            switch(type) {
            case 0: return stat_.st_size;
            case 1: return 0;
            case 2: return stat_.st_size;
            }
            assert(false);
            throw std::runtime_error{"this shouldn't happen"};
        }();
//        std::cout << "adding file " << path << " as " << newName << " type: " << (int)type << " size: " << size << "\n";
        auto state = EntryHeader {
            .uid       = 0,
            .gid       = 0,
            .type      = type,
            .perms     = perms,
            .size      = size,
            .name_size = newNameAsStr.size(),
        };
        addPOD(state);
        addInfo(newNameAsStr);
        if (type == 0) { // is file
            auto ifs = std::ifstream{path, std::ios::binary};
            auto buffer = std::vector<char>(65536);
            while (!ifs.eof()) {
                ifs.read(buffer.data(), buffer.size());
                ofs.write(buffer.data(), ifs.gcount());
            }
        } else if (type == 2) { // is symlink
            auto symlink = read_symlink(path).string();
            ofs.write(symlink.data(), symlink.size());
        }
    }

    void addInfo(std::span<char const> data) {
        ofs.write(data.data(), data.size());
    }

    template <typename T>
    void addPOD(T const& t) {
        addInfo(std::span{reinterpret_cast<char const*>(&t), sizeof(T)});
    }
};
}
