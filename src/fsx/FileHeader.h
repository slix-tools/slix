// SPDX-FileCopyrightText: 2023 S. G. Gottlieb <info.simon@gottliebtfreitag.de>
// SPDX-License-Identifier: AGPL-3.0-only
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
