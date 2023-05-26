#pragma once
#include <cstdint>

namespace fsx {
struct __attribute__((__packed__)) EntryHeader {
    uint32_t uid;
    uint32_t gid;
    uint8_t  type;
    uint16_t perms;
    uint64_t size;
    uint64_t name_size;
};
}
