// SPDX-FileCopyrightText: 2023 S. G. Gottlieb <info.simon@gottliebtfreitag.de>
// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include <clice/clice.h>

inline auto cliVerbose = clice::Argument{ .args   = {"--verbose", "-v"},
                                          .desc   = "detailed description of what is happening",
};
