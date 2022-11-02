// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2022 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include "helpers.h"
#include "package.h"

namespace sw {

struct any_setting {
    static constexpr auto name = "any_setting"sv;
};

namespace os {

struct windows {
    static constexpr auto name = "windows"sv;

    static constexpr auto executable_extension = ".exe";
    static constexpr auto object_file_extension = ".obj";
    static constexpr auto static_library_extension = ".lib";
    static constexpr auto shared_library_extension = ".dll";

    // deps:
    // kernel32 dependency (winsdk.um)
};

struct mingw : windows {
    static constexpr auto name = "mingw"sv;
};

struct cygwin : windows {
    static constexpr auto name = "cygwin"sv;

    static constexpr auto static_library_extension = ".a";
    static constexpr auto object_file_extension = ".o";
};

struct unix {
    static constexpr auto object_file_extension = ".o";
    static constexpr auto static_library_extension = ".a";
};

struct linux : unix {
    static constexpr auto name = "linux"sv;

    static constexpr auto shared_library_extension = ".so";
};

struct darwin : unix {
    static constexpr auto shared_library_extension = ".dylib";
};

struct macos : darwin {
    static constexpr auto name = "macos"sv;
};
// ios etc

struct wasm : unix {
    static constexpr auto name = "wasm"sv;

    static constexpr auto executable_extension = ".html";
};

} // namespace os

namespace arch {

struct x86 {
    static constexpr auto name = "x86"sv;
};
struct x64 {
    static constexpr auto name = "x64"sv;
};
using amd64 = x64;
using x86_64 = x64;

struct arm {
    static constexpr auto name = "arm"sv;
};
struct arm64 {
    static constexpr auto name = "arm64"sv;
};
using aarch64 = arm64;

} // namespace arch

namespace build_type {

struct debug {
    static constexpr auto name = "debug"sv;
};
struct minimum_size_release {
    static constexpr auto name = "minimum_size_release"sv;
};
struct release_with_debug_information {
    static constexpr auto name = "release_with_debug_information"sv;
};
struct release {
    static constexpr auto name = "release"sv;
};

} // namespace build_type

namespace library_type {

struct static_ {
    static constexpr auto name = "static"sv;
};
struct shared {
    static constexpr auto name = "shared"sv;
};

} // namespace library_type

struct compiler_base {
    unresolved_package_name name;

    compiler_base(const unresolved_package_name &name) : name{name} {}
};
struct clang_base : compiler_base {
    static constexpr auto name = "clang"sv;
    using compiler_base::compiler_base;
    clang_base() : compiler_base{unresolved_package_name{"org.llvm.clang", "*"}} {
    }

    void detect(auto &sln) {
    }
};
struct gcc_base : compiler_base {
    static constexpr auto name = "gcc"sv;
    using compiler_base::compiler_base;
    gcc_base() : compiler_base{unresolved_package_name{"org.gnu.gcc", "*"}} {
    }

    void detect(auto &sln) {
    }
};
struct msvc_base : compiler_base {
    static constexpr auto name = "msvc"sv;
    using compiler_base::compiler_base;
    msvc_base() : compiler_base{unresolved_package_name{"com.Microsoft.VisualStudio.VC.cl", "*"}} {
    }

    void detect(auto &sln) {
    }
};

namespace asm_compiler {

struct clang {
    static constexpr auto name = "clang"sv;
};
struct gcc {
    static constexpr auto name = "gcc"sv;
};
struct msvc {
    static constexpr auto name = "msvc"sv;
};

} // namespace c_compiler

namespace c_compiler {

struct clang : clang_base {
    using clang_base::clang_base;
};
struct gcc : gcc_base {
    using gcc_base::gcc_base;
};
struct msvc : msvc_base {
    using msvc_base::msvc_base;
};

} // namespace c_compiler

namespace cpp_compiler {

struct clang : clang_base {
    using clang_base::clang_base;
};
struct gcc : gcc_base {
    using gcc_base::gcc_base;
};
struct msvc : msvc_base {
    using msvc_base::msvc_base;
};

} // namespace cpp_compiler

namespace objc_compiler {

struct apple_clang {
    static constexpr auto name = "apple_clang"sv;
};
struct clang {
    static constexpr auto name = "clang"sv;
};
struct gcc {
    static constexpr auto name = "gcc"sv;
};

} // namespace objc_compiler

namespace objcpp_compiler {

struct apple_clang {
    static constexpr auto name = "apple_clang"sv;
};
struct clang {
    static constexpr auto name = "clang"sv;
};
struct gcc {
    static constexpr auto name = "gcc"sv;
};

} // namespace objcpp_compiler

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
    cpp_compiler_type cpp_compiler;

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
    static auto default_build_settings() {
        build_settings bs;
        bs.build_type = build_type::release{};
        bs.library_type = library_type::shared{};

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

        // see more definitions at https://opensource.apple.com/source/WTF/WTF-7601.1.46.42/wtf/Platform.h.auto.html
#if defined(_WIN32)
        bs.os = os::windows{};
        //bs.c_compiler = c_compiler::msvc{}; // switch to gcc-12+
        //bs.cpp_compiler = cpp_compiler::msvc{}; // switch to gcc-12+
#elif defined(__APPLE__)
        bs.os = os::macos{};
        //bs.c_compiler = c_compiler::gcc{}; // gcc-12+
        //bs.cpp_compiler = cpp_compiler::gcc{}; // gcc-12+
#elif defined(__linux__)
        bs.os = os::linux{};
        //bs.c_compiler = c_compiler::gcc{};
        //bs.cpp_compiler = cpp_compiler::gcc{};
#else
#error "unknown os"
#endif

        return bs;
    }
};

} // namespace sw
