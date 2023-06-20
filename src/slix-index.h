#pragma once

#include <clice/clice.h>

inline auto cliIndex = clice::Argument{ .args   = "index",
                                        .desc   = "sub command to operate on some index"
};
