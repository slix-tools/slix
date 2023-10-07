// SPDX-FileCopyrightText: 2023 S. G. Gottlieb <info.simon@gottliebtfreitag.de>
// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include <fmt/format.h>
#include <fmt/std.h>
#include <stdexcept>

struct error_fmt : std::exception {
    std::string message;

    template <typename... Args>
    error_fmt(fmt::format_string<Args...> s, Args&&... args) {
        message = fmt::format(s, std::forward<Args>(args)...);
    }

    auto what() const noexcept -> char const* override {
        return message.c_str();
    }
};
