// SPDX-FileCopyrightText: 2023 S. G. Gottlieb <info.simon@gottliebtfreitag.de>
// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include "FileHeader.h"
#include "EntryHeader.h"

#include <filesystem>
#include <fstream>
#include <optional>
#include <span>
#include <stdexcept>
#include <vector>

namespace fsx {

struct Reader {
    std::ifstream ifs;

    Reader(std::filesystem::path path_)
        : ifs{path_, std::ios::binary}
    {
        if (!ifs.good()) {
            throw std::runtime_error{"could not open file: " + path_.string()};
        }
        auto defaultFileHeader = fsx::FileHeader{};
        auto header = readPOD<fsx::FileHeader>();
        if (header != defaultFileHeader) {
            throw std::runtime_error{"unexpected header"};
        }
    }
    Reader(Reader const&) = delete;
    Reader(Reader&&) noexcept = default;

    struct Entry {
        EntryHeader header;
        std::string name;
        size_t      file_offset;

    };
    auto readNext() -> std::optional<Entry> {
        auto entry = Entry{};
        ifs.read(reinterpret_cast<char*>(&entry.header), sizeof(entry.header));
        if (ifs.gcount() != sizeof(entry.header)) return std::nullopt;

        entry.name.resize(entry.header.name_size);
        ifs.read(entry.name.data(), entry.header.name_size);
        if (ifs.gcount() != entry.header.name_size) return std::nullopt;
        entry.file_offset = ifs.tellg();
        ifs.seekg((uint64_t)ifs.tellg() + entry.header.size); //!TODO tellg could be negative
        return entry;
    }

    auto readContent(char* buf, size_t count, size_t offset) -> size_t {
        ifs.clear();
        ifs.seekg(offset);
        ifs.read(buf, count);
        size_t ct = ifs.gcount();
        return ct;
    }

    template <typename T>
    auto readPOD() -> T {
        T t;
        ifs.read(reinterpret_cast<char*>(&t), sizeof(t));
        return t;
    }
};
}
