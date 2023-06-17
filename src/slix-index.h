#pragma once

#include <clice/clice.h>

inline auto cliIndex = clice::Argument{ .arg    = "index",
                                        .desc   = "sub command to operate on some index"
};
