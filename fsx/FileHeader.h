#pragma once

#include <array>

namespace fsx {
struct FileHeader {
    std::array<char, 6> magicBytes {'F', 'S', 'X', '-', 'A', 'R'};
    char                major{0};
    char                minor{0};

    auto operator<=>(FileHeader const&) const noexcept = default;
};
}
