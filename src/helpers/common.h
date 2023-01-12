// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2022 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include "crypto/common.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <cassert>
#include <charconv>
#include <cmath>
#include <deque>
#include <filesystem>
#if __has_include(<format>)
#include <format>
using std::format;
namespace fmt = std;
#elif __has_include(<format.h>)
#define FMT_HEADER_ONLY
#include <format.h>
using fmt::format;
#else
#define FMT_HEADER_ONLY
#include <fmt/format.h>
using fmt::format;
#endif
#include <fstream>
#include <functional>
#include <iostream>
#include <limits>
#include <map>
#include <mutex>
#include <ranges>
#include <regex>
#include <set>
#include <source_location>
#include <span>
#include <thread>
#include <unordered_set>
#include <variant>

#if defined(_MSC_VER)
#if _MSC_VER < 1934
#error "use VS 17.4 or later"
#endif
#elif defined(__GNUC__)
#if __GNUC__ < 12
#error "use gcc 12 or later"
#endif
#else
#error "compiler is not working or not tested"
#endif

namespace sw {

namespace fs = std::filesystem;
using path = fs::path;

} // namespace sw

template <>
struct fmt::formatter<std::source_location> : formatter<std::string> {
    auto format(const std::source_location &p, format_context &ctx) {
        return fmt::formatter<std::string>::format(fmt::format("{}:{}", p.file_name(), p.line()), ctx);
    }
};
template <>
struct fmt::formatter<sw::path> : formatter<std::string> {
    auto format(const sw::path &p, format_context &ctx) {
        return fmt::formatter<std::string>::format(p.string(), ctx);
    }
};

//#define SW_BINARY_DIR ".sw"

namespace sw {

namespace fs = std::filesystem;
using path = fs::path;
using std::string;
using std::string_view;
using std::variant;
using std::vector;
using namespace std::literals;

template <typename ... Types>
using uptr = std::unique_ptr<Types...>;

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
template <typename ... Types> struct types {
    using variant_type = variant<Types...>;
    using variant_of_ptr_type = variant<Types*...>;
    using variant_of_uptr_type = variant<uptr<Types>...>;

    template <typename T>
    static constexpr bool contains() {
        return (false || ... || std::same_as<Types, T>);
    }
    static auto for_each(auto &&f) {
        return (f((Types**)nullptr) || ... || false);
    }
};

#ifndef FWD
#define FWD(x) std::forward<decltype(x)>(x)
#endif

template <typename... Ts>
struct overload : Ts... {
    overload(Ts... ts) : Ts(FWD(ts))... {    }
    using Ts::operator()...;
};

decltype(auto) visit(auto &&var, auto && ... f) {
    return ::std::visit(overload{FWD(f)...}, var);
}
decltype(auto) visit_any(auto &&var, auto &&...f) {
    return visit(FWD(var), overload{FWD(f)..., [](auto &&){}});
}
decltype(auto) visit1(auto &&var, auto &&...f) {
    return overload{FWD(f)...}(FWD(var));
}

template <typename T, typename Head, typename... Types>
static constexpr bool contains() {
    if constexpr (sizeof...(Types) == 0) {
        return std::same_as<T, Head>;
    } else {
        return std::same_as<T, Head> || contains<T, Types...>();
    }
}
template <typename T, template <typename...> typename Container, typename... Types>
static constexpr bool contains(Container<Types...> **) {
    return contains<T, Types...>();
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

struct abspath : path {
    using base = path;

    abspath() = default;
    abspath(const base &p) : base{p} {
        init();
    }
    abspath(auto &&p) : base{p} {
        init();
    }
    abspath(const abspath &) = default;

    void init() {
        // we need target os concept and do this when needed
/*#ifdef _WIN32
        std::wstring s = fs::absolute(p);
        std::transform(s.begin(), s.end(), s.begin(), towlower);
        base::operator=(s);
#else*/
        base::operator=(fs::absolute(*this));
        base::operator=(fs::absolute(*this).lexically_normal()); // on linux absolute does not remove last '.'?
//#endif
    }
};

struct unimplemented_exception : std::runtime_error {
    unimplemented_exception(std::source_location sl = std::source_location::current())
        : runtime_error{format("unimplemented: {}", sl)} {}
};
#define SW_UNIMPLEMENTED throw unimplemented_exception{}

string &&normalize_path(string &&s) {
    std::replace(s.begin(), s.end(), '\\', '/');
    return std::move(s);
}
void lower_drive_letter(string &s) {
    if (!s.empty()) {
        s[0] = tolower(s[0]);
    }
}
void mingw_drive_letter(string &s) {
    if (s.size() > 1 && s[1] == ':') {
        s[0] = tolower(s[0]);
        s = "/"s + s[0] + s.substr(2);
    }
}
auto normalize_path(const path &p) {
    auto fn = p.string();
    std::replace(fn.begin(), fn.end(), '\\', '/');
    return fn;
}
auto normalize_path_and_drive(const path &p) {
    auto fn = normalize_path(p);
    lower_drive_letter(fn);
    return fn;
}

template<std::size_t N>
struct static_string {
    char p[N]{};
    constexpr static_string(char const(&pp)[N]) {
        std::ranges::copy(pp, p);
    }
    operator auto() const { return &p[0]; }
    operator string_view() const { return string_view{p, N-1}; }
};
template<static_string s>
constexpr auto operator""_s() { return s; }

static bool is_mingw_shell() {
    static auto b = getenv("MSYSTEM");
    return b;
}

template <typename T>
struct swap_and_restore {
    T &restore;
    T original_value;
    bool should_restore{true};

public:
    swap_and_restore(T &restore) : swap_and_restore(restore, restore) {
    }
    template <typename U>
    swap_and_restore(T &restore, U NewVal) : restore(restore), original_value(restore) {
        restore = std::move(NewVal);
    }
    ~swap_and_restore() {
        if (should_restore)
            restore = std::move(original_value);
    }
    void restore_now(bool force) {
        if (!force && !should_restore)
            return;
        restore = std::move(original_value);
        should_restore = false;
    }

    swap_and_restore(const swap_and_restore &) = delete;
    swap_and_restore &operator=(const swap_and_restore &) = delete;
};

auto read_file(const path &fn) {
    auto sz = fs::file_size(fn);
    string s(sz, 0);
    FILE *f = fopen(fn.string().c_str(), "rb");
    fread(s.data(), s.size(), 1, f);
    fclose(f);
    return s;
}

void write_file(const path &fn, const string &s = {}) {
    fs::create_directories(fn.parent_path());

    FILE *f = fopen(fn.string().c_str(), "wb");
    fwrite(s.data(), s.size(), 1, f);
    fclose(f);
}

void write_file_if_different(const path &fn, const string &s) {
    if (fs::exists(fn) && read_file(fn) == s) {
        return;
    }
    write_file(fn, s);
}

size_t fnv1a(auto &&in) {
    auto hash = 0xcbf29ce484222325ULL;
    for (auto &&byte : in) {
        hash = hash ^ byte;
        hash = hash * 0x100000001b3ULL;
    }
    return hash;
}

template <class T>
inline size_t hash_combine(size_t &seed, const T &v) {
    auto distribute = [](size_t n) {
        auto xorshift = [](size_t n, int i) {
            return n ^ (n >> i);
        };
        uint64_t p = 0x5555555555555555ull;
        uint64_t c = 17316035218449499591ull; // use fnv1a prime?
        return c * xorshift(p * xorshift(n, 32), 32);
    };
    return std::rotl(seed, std::numeric_limits<size_t>::digits / 3) ^ distribute(std::hash<T>{}(v));
}

string to_upper_copy(string in) {
    std::transform(in.begin(), in.end(), in.begin(), toupper);
    return std::move(in);
}

void write_file_once(const path &fn, const string &content, const path &lock_dir) {
    auto sha3 = [](auto &&d) {
        return digest<crypto::sha3<256>>(d);
    };

    auto h = sha3(content);

    auto hf = sha3(normalize_path(fn));
    const auto once = lock_dir / (hf + ".once");
    const auto lock = lock_dir / hf;

    if (!fs::exists(once) || h != read_file(once) || !fs::exists(fn)) {
        //ScopedFileLock fl(lock);
        write_file_if_different(fn, content);
        write_file_if_different(once, h);
    }
}

path resolve_executable(auto &&exe) {
#ifdef _WIN32
    auto p = getenv("Path");
    auto delim = ';';
    auto exts = {".exe", ".bat", ".cmd", ".com"};
#else
    auto p = getenv("PATH");
    auto delim = ':';
    auto exts = {""};
#endif
    if (!p) {
        return {};
    }
    string p2 = p;
    for (const auto word : std::views::split(p2, delim) | std::views::transform([](auto &&word) {
                               return std::string_view{word.begin(), word.end()};
                           })) {
        auto p = path{word} / exe;
        for (auto &e : exts) {
            if (fs::exists(path{p} += e)) {
                return path{p} += e;
            }
        }
    }
    return {};
}

void append_vector(auto &&to, auto &&from) {
    to.reserve(to.size() + from.size());
    for (auto &&v : from) {
        to.push_back(v);
    }
}

auto is_c_file(const path &fn) {
    static std::set<string> exts{".c", ".m"}; // with obj-c, separate call?
    return exts.contains(fn.extension().string());
}
auto is_cpp_file(const path &fn) {
    static std::set<string> exts{".cpp", ".cxx", ".mm"}; // with obj-c++, separate call?
    return exts.contains(fn.extension().string());
}

} // namespace sw

template <>
struct std::hash<::sw::abspath> {
    size_t operator()(const ::sw::abspath &p) {
        return std::hash<sw::path>()(p);
    }
};
