// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2022 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include <algorithm>
#if __has_include(<format>)
#include <format>
using std::format;
namespace fmt = std;
#elif __has_include(<format.h>)
#error "wrong branch"
#define FMT_HEADER_ONLY
#include <format.h>
using fmt::format;
#else
#define FMT_HEADER_ONLY
#include <fmt/chrono.h>
#include <fmt/format.h>
using fmt::format;
#endif
#include <source_location>

template <>
struct fmt::formatter<std::source_location> : formatter<std::string> {
    auto format(const std::source_location &p, format_context &ctx) const {
        return fmt::formatter<std::string>::format(fmt::format("{}:{}", p.file_name(), p.line()), ctx);
    }
};


// #define SW_BINARY_DIR ".sw"

namespace sw {

using std::string;
using std::string_view;
using namespace std::literals;

[[nodiscard]] inline std::string replace(const std::string &str, const std::string &oldstr, const std::string &newstr,
                                         int count = -1) {
    int sofar = 0;
    int cursor = 0;
    string s(str);
    string::size_type oldlen = oldstr.size(), newlen = newstr.size();
    cursor = s.find(oldstr, cursor);
    while (cursor != -1 && cursor <= (int)s.size()) {
        if (count > -1 && sofar >= count) {
            break;
        }
        s.replace(cursor, oldlen, newstr);
        cursor += (int)newlen;
        if (oldlen != 0) {
            cursor = s.find(oldstr, cursor);
        } else {
            ++cursor;
        }
        ++sofar;
    }
    return s;
}

template <std::size_t N>
struct static_string {
    char p[N]{};
    constexpr static_string(char const (&pp)[N]) {
        std::ranges::copy(pp, p);
    }
    operator auto() const {
        return &p[0];
    }
    operator string_view() const {
        return string_view{p, N - 1};
    }
};
template <static_string s>
constexpr auto operator""_s() {
    return s;
}

string to_upper_copy(string in) {
    std::transform(in.begin(), in.end(), in.begin(), ::toupper);
    return std::move(in);
}

} // namespace sw
