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

std::string get_severity_string(int s) {
    if (s >= 6) return "trace";
    if (s >= 4) return "debug";
    if (s == 0) return "info";
    if (s <= -6) return "fatal";
    if (s <= -4) return "error";
    if (s <= -2) return "warn";
    return std::to_string(s);
}
void log(const char *compponent, int severity, auto &&fmtstring, auto &&...args) {
    std::cerr << fmt::format("[{}][{}] {}\n", std::chrono::system_clock::now(), get_severity_string(severity), fmt::vformat(fmtstring, fmt::make_format_args(FWD(args)...)));
}

void log_fatal(auto &&fmtstring, auto &&...args) {
    log("test", -6, fmtstring, FWD(args)...);
}
void log_error(auto &&fmtstring, auto &&...args) {
    log("test", -4, fmtstring, FWD(args)...);
}
void log_warn(auto &&fmtstring, auto &&...args) {
    log("test", -2, fmtstring, FWD(args)...);
}
void log_debug(auto &&fmtstring, auto &&...args) {
    log("test", 4, fmtstring, FWD(args)...);
}
void log_trace(auto &&fmtstring, auto &&...args) {
    log("test", 6, fmtstring, FWD(args)...);
}

} // namespace sw
