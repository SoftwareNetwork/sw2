// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2022 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include "helpers.h"
#include "package.h"
#include "os_base.h"
#include "detect.h"

namespace sw {

struct build_settings {
    template <typename ... Types>
    struct special_variant : variant<any_setting, Types...> {
        using base = variant<any_setting, Types...>;
        using base::base;
        using base::operator=;

        auto operator<(const special_variant &rhs) const {
            return base::index() < rhs.base::index();
        }
        //auto operator==(const build_settings &) const = default;

        template <typename T>
        bool is() const {
            return contains<T, Types...>();
        }
    };

    using os_type = special_variant<os::linux, os::macos, os::windows, os::mingw, os::cygwin, os::wasm>;
    using arch_type = special_variant<arch::x86, arch::x64, arch::arm, arch::arm64>;
    using build_type_type = special_variant<build_type::debug, build_type::minimum_size_release,
        build_type::release_with_debug_information, build_type::release>;
    using library_type_type = special_variant<library_type::static_, library_type::shared>;
    using c_compiler_type = special_variant<c_compiler::clang, c_compiler::gcc, c_compiler::msvc>;
    using cpp_compiler_type = special_variant<cpp_compiler::clang, cpp_compiler::gcc, cpp_compiler::msvc>;

    os_type os;
    arch_type arch;
    build_type_type build_type;
    library_type_type library_type;
    c_compiler_type c_compiler;
    // move under cpp. struct?
    cpp_compiler_type cpp_compiler;
    unresolved_package_name cpp_stdlib;

    auto for_each() const {
        return std::tie(os, arch, build_type, library_type);
    }
    auto for_each(auto &&f) const {
        std::apply(
            [&](auto &&...args) {
                (f(FWD(args)), ...);
            },
            for_each());
    }

    void visit(auto &&...f) {
        for_each([&](auto && a) {
            visit_any(a, FWD(f)...);
        });
    }

    template <typename T>
    bool is1() const {
        int cond = 0;
        for_each([&](auto &&a) {
            if (cond == 1) {
                return;
            }
            if constexpr (contains<T>((std::decay_t<decltype(a)> **)nullptr)) {
                cond += std::holds_alternative<T>(a);
            }
        });
        return cond == 1;
    }
    template <typename T, typename ... Types>
    bool is() const {
        return (is1<T>() && ... && is1<Types>());
    }

    size_t hash() const {
        size_t h = 0;
        for_each([&](auto &&a) {
            ::sw::visit(a, [&](auto &&v) {
                h ^= std::hash<string_view>()(v.name);
            });
        });
        return h;
    }

    auto operator<(const build_settings &rhs) const {
        return for_each() < rhs.for_each();
    }

    static auto host_os() {
        build_settings bs;
        // these will be set for custom linux distribution
        //bs.build_type = build_type::release{};
        //bs.library_type = library_type::shared{};

        // see more definitions at https://opensource.apple.com/source/WTF/WTF-7601.1.46.42/wtf/Platform.h.auto.html
#if defined(__MINGW32__)
        bs.os = os::mingw{};
#elif defined(_WIN32)
        bs.os = os::windows{};
#elif defined(__APPLE__)
        bs.os = os::macos{};
#elif defined(__linux__)
        bs.os = os::linux{};
#else
#error "unknown os"
#endif

#if defined(__x86_64__) || defined(_M_X64)
        bs.arch = arch::x64{};
#elif defined(__i386__) || defined(_M_IX86)
        bs.arch = arch::x86{};
#elif defined(__arm64__) || defined(__aarch64__)
        bs.arch = arch::arm64{};
#elif defined(__arm__)
        bs.arch = arch::arm{};
#else
#error "unknown arch"
#endif

        return bs;
    }
};

} // namespace sw
