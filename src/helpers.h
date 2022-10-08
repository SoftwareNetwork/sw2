// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2022 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include <algorithm>
#include <atomic>
#include <cassert>
#include <filesystem>
/*#if __has_include(<format>)
#include <format>
using std::format;
#elif __has_include(<format.h>)
#include <format.h>
using fmt::format;
#else
#include <fmt/format.h>
using fmt::format;
#endif*/
#include <fstream>
#include <functional>
#include <iostream>
#include <limits>
#include <map>
#include <ranges>
#include <regex>
#include <set>
#include <unordered_set>
#include <variant>

namespace fs = std::filesystem;
using path = fs::path;
using std::string;
using std::string_view;
using std::variant;
using namespace std::literals;

template <typename F>
struct appender {
    F f;
    auto operator,(auto &&v) {
        f(v);
        return std::move(*this);
    }
};

template <typename T>
struct iter_with_range {
    T range;
    std::ranges::iterator_t<T> i;

    iter_with_range(T &&r) : range{r}, i{range.begin()} {}
    auto operator*() const { return *i; }
    void operator++() { ++i; }
    bool operator==(auto &&) const {
        return i == std::end(range);
    }
};

template <typename F>
struct scope_exit {
    F &&f;
    ~scope_exit() {f();}
};

template <typename T> struct type_ { using type = T; };
template <typename ... Args> struct types {
    template <typename T>
    static constexpr bool contains() {
        return (false || ... || std::same_as<Args, T>);
    }
};
template <typename ... Args>
constexpr auto make_variant(types<Args...>) {return type_<std::variant<Args...>>{};}

#ifndef FWD
#define FWD(x) std::forward<decltype(x)>(x)
#endif

template <typename... Ts>
struct overload : Ts... {
    overload(Ts... ts) : Ts(FWD(ts))... {    }
    using Ts::operator()...;
};

decltype(auto) visit(auto &&var, auto && ... f) {
    return std::visit(overload{f...}, var);
}

[[nodiscard]] std::string replace(const std::string &str, const std::string &oldstr, const std::string &newstr,
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
