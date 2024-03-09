// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2022 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include "../sys/string.h"

#include "../sys/fs2.h"
#include "../crypto/common.h"

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
#error "wrong branch"
#define FMT_HEADER_ONLY
#include <format.h>
using fmt::format;
#else
#define FMT_HEADER_ONLY
#include <fmt/format.h>
#include <fmt/chrono.h>
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

#ifndef _WIN32
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#endif

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

//#define SW_BINARY_DIR ".sw"

namespace sw {

using std::variant;
using std::vector;

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
    bool disabled{};
    ~scope_exit() {if(!disabled)f();}
    void disable() {disabled = true;}
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

void append_vector(auto &&to, auto &&from) {
    to.reserve(to.size() + from.size());
    for (auto &&v : from) {
        to.push_back(v);
    }
}

//
struct any_setting {
    static constexpr auto name = "any_setting"sv;

    auto operator<=>(const any_setting &) const = default;
};

template <typename... Types>
struct special_variant : variant<any_setting, Types...> {
    using base = variant<any_setting, Types...>;
    using base::base;
    using base::operator=;

    auto operator<(const special_variant &rhs) const {
        return base::index() < rhs.base::index();
    }
    // auto operator==(const build_settings &) const = default;

    template <typename T>
    bool is() const {
        constexpr auto c = contains<T, Types...>();
        if constexpr (c) {
            return std::holds_alternative<T>(*this);
        }
        return false;
    }

    auto for_each(auto &&f) {
        (f(Types{}), ...);
    }

    decltype(auto) visit(auto &&...args) const {
        return ::sw::visit(*this, FWD(args)...);
    }
    decltype(auto) visit_any(auto &&...args) const {
        return ::sw::visit_any(*this, FWD(args)...);
    }
    // name visit special or?
    decltype(auto) visit_no_special(auto &&...args) {
        return ::sw::visit(*this, FWD(args)..., [](any_setting &) {
        });
    }
    decltype(auto) visit_no_special(auto &&...args) const {
        return ::sw::visit(*this, FWD(args)..., [](const any_setting &) {
        });
    }
};

struct cpp_emitter {
    struct ns {
        cpp_emitter &e;
        ns(cpp_emitter &e, auto &&name) : e{e} {
            e += "namespace "s + name + " {";
        }
        ~ns() {
            e += "}";
        }
    };

    string s;
    int indent{};

    cpp_emitter &operator+=(auto &&s) {
        add_line(s);
        return *this;
    }
    void add_line(auto &&s) {
        this->s += s + "\n"s;
    }
    void include(const path &p) {
        auto fn = normalize_path_and_drive(p);
        s += "#include \"" + fn + "\"\n";
    }
    auto namespace_(auto &&name) {
        return ns{*this, name};
    }
};

// refine
path get_this_file_dir() {
    path p = __FILE__;
    if (p.is_absolute()) {
        return p.parent_path();
    } else {
        auto swdir = fs::absolute(path{std::source_location::current().file_name()}.parent_path());
        return swdir;
    }
}
path get_sw_dir() {
    path p = get_this_file_dir();
    return p.parent_path().parent_path();
}

} // namespace sw
