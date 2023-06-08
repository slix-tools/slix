#pragma once

#include <clice/clice.h>

inline auto cliVerbose = clice::Argument{ .arg    = {"--verbose"},
                                          .desc   = "detailed description of what is happening",
};
