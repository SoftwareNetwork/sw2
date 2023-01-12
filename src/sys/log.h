// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2022 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include "mmap.h"

namespace sw {

struct log_file {
    mmap_file<char> f;

    log_file(const path &fn) : f{fn} {
    }
};

constexpr void log(const char *compponent, int severity, auto &&fmtstring, auto &&...args) {
    std::cerr << fmt::format("[{}][{}] {}\n", std::chrono::system_clock::now(), severity, fmt::vformat(fmtstring, fmt::make_format_args(FWD(args)...))) << "\n";
}
constexpr void log_debug(auto &&fmtstring, auto &&...args) {
    log("test", 4, fmtstring, FWD(args)...);
}
constexpr void log_trace(auto &&fmtstring, auto &&...args) {
    //log("test", 6, fmtstring, FWD(args)...);
}

} // namespace sw
