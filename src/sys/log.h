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

// log sink
// log settings per sink
struct log_settings_type {
    int log_level{};
};
log_settings_type log_settings;

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
    if (log_settings.log_level < severity) {
        return;
    }
    string s;
    if constexpr (sizeof...(args) > 0) {
        s = fmt::vformat(fmtstring, fmt::make_format_args(FWD(args)...));
    } else {
        s += fmtstring;
    }
    std::cerr << fmt::format("[{}] [{}] {}\n"
#ifdef _MSC_VER
        , std::chrono::system_clock::now()
#else
        , "not impl"
#endif
        , get_severity_string(severity), s);
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
void log_info(auto &&fmtstring, auto &&...args) {
    log("test", 0, fmtstring, FWD(args)...);
}
void log_debug(auto &&fmtstring, auto &&...args) {
    log("test", 4, fmtstring, FWD(args)...);
}
void log_trace(auto &&fmtstring, auto &&...args) {
    log("test", 6, fmtstring, FWD(args)...);
}

} // namespace sw
