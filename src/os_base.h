// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2022 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include "helpers.h"
#include "package.h"
#include "rule_list.h"

namespace sw {

//
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

    static bool is(string_view sv) {
        return name == sv;
    }

    // deps:
    // kernel32 dependency (winsdk.um)
};

struct mingw : windows {
    static constexpr auto name = "mingw"sv;

    static bool is(string_view sv) {
        return name == sv;
    }
};

struct cygwin : windows {
    static constexpr auto name = "cygwin"sv;

    static constexpr auto static_library_extension = ".a";
    static constexpr auto object_file_extension = ".o";

    static bool is(string_view sv) {
        return name == sv;
    }
};

struct unix {
    static constexpr auto object_file_extension = ".o";
    static constexpr auto static_library_extension = ".a";
};

struct linux : unix {
    static constexpr auto name = "linux"sv;

    static constexpr auto shared_library_extension = ".so";

    static bool is(string_view sv) {
        return name == sv;
    }
};

struct darwin : unix {
    static constexpr auto shared_library_extension = ".dylib";
};

struct macos : darwin {
    static constexpr auto name = "macos"sv;

    static bool is(string_view sv) {
        return name == sv;
    }
};
// ios etc

struct wasm : unix {
    static constexpr auto name = "wasm"sv;

    static constexpr auto executable_extension = ".html";

    static bool is(string_view sv) {
        return name == sv;
    }
};

} // namespace os

namespace arch {

struct x86 {
    static constexpr auto name = "x86"sv;
    static constexpr auto clang_target_name = "i586"sv; // but also 386, 586, 686

    static bool is(string_view sv) {
        return name == sv;
    }
};
struct x64 {
    static constexpr auto name = "x64"sv;
    static constexpr auto name1 = "x86_64"sv;
    static constexpr auto name2 = "amd64"sv;
    static constexpr auto clang_target_name = name1;

    static bool is(string_view sv) {
        return 0
            || sv == name
            || sv == name1
            || sv == name2
            ;
    }
};
using amd64 = x64;
using x86_64 = x64;

struct arm {
    static constexpr auto name = "arm"sv;
    static constexpr auto clang_target_name = name;

    static bool is(string_view sv) {
        return name == sv;
    }
};
struct arm64 {
    static constexpr auto name = "arm64"sv;
    static constexpr auto clang_target_name = name;

    static bool is(string_view sv) {
        return name == sv;
    }
};
using aarch64 = arm64; // give alternative names

} // namespace arch

namespace build_type {

struct debug {
    static constexpr auto name = "debug"sv;
    static constexpr auto short_name = "d"sv;

    static bool is(string_view sv) {
        return name == sv || short_name == sv;
    }
};
struct minimum_size_release {
    static constexpr auto name = "minimum_size_release"sv;
    static constexpr auto short_name = "msr"sv;

    static bool is(string_view sv) {
        return name == sv || short_name == sv;
    }
};
struct release_with_debug_information {
    static constexpr auto name = "release_with_debug_information"sv;
    static constexpr auto short_name = "rwdi"sv;

    static bool is(string_view sv) {
        return name == sv || short_name == sv;
    }
};
struct release {
    static constexpr auto name = "release"sv;
    static constexpr auto short_name = "r"sv;

    static bool is(string_view sv) {
        return name == sv || short_name == sv;
    }
};

} // namespace build_type

namespace library_type {

struct static_ {
    static constexpr auto name = "static"sv;
    static constexpr auto short_name = "st"sv;
};
struct shared {
    static constexpr auto name = "shared"sv;
    static constexpr auto short_name = "sh"sv;
};

} // namespace library_type

struct compiler_base {
    unresolved_package_name package;

    compiler_base(const unresolved_package_name &name) : package{name} {}
};
struct clang_base : compiler_base {
    static constexpr auto name = "clang"sv;
    using compiler_base::compiler_base;
    clang_base(const unresolved_package_name &name) : compiler_base{name} {
    }

    static bool is(string_view sv) {
        return name == sv;
    }
};
struct gcc_base : compiler_base {
    static constexpr auto name = "gcc"sv;
    using compiler_base::compiler_base;
    gcc_base(const unresolved_package_name &name) : compiler_base{name} {
    }

    static bool is(string_view sv) {
        return name == sv;
    }
};
struct msvc_base : compiler_base {
    static constexpr auto name = "msvc"sv;
    using compiler_base::compiler_base;
    msvc_base() : compiler_base{unresolved_package_name{"com.Microsoft.VisualStudio.VC.cl", "*"}} {
    }

    static bool is(string_view sv) {
        return name == sv;
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
    using rule_type = gcc_compile_rule;
    static inline constexpr auto package_name = "org.llvm.clang";

    clang() : clang_base{unresolved_package_name{package_name, "*"}} {
    }
};
struct gcc : gcc_base {
    using rule_type = gcc_compile_rule;
    static inline constexpr auto package_name = "org.gnu.gcc";

    gcc() : gcc_base{unresolved_package_name{package_name, "*"}} {
    }
};
struct msvc : msvc_base {
    using msvc_base::msvc_base;
    using rule_type = cl_exe_rule;
};

} // namespace c_compiler

namespace cpp_compiler {

struct clang : clang_base {
    using rule_type = gcc_compile_rule;
    static inline constexpr auto package_name = "org.llvm.clang++";

    clang() : clang_base{unresolved_package_name{package_name, "*"}} {
    }
};
struct gcc : gcc_base {
    using rule_type = gcc_compile_rule;
    static inline constexpr auto package_name = "org.gnu.g++";

    gcc() : gcc_base{unresolved_package_name{package_name, "*"}} {
    }
};
struct msvc : msvc_base {
    using msvc_base::msvc_base;
    using rule_type = cl_exe_rule;
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

namespace librarian {

struct msvc {
    using rule_type = lib_exe_rule;
    unresolved_package_name package{"com.Microsoft.VisualStudio.VC.lib"s};
};
struct ar {
    using rule_type = lib_ar_rule;
    unresolved_package_name package{"org.gnu.binutils.ar"s};
};

} // namespace librarian

namespace linker {

struct msvc {
    using rule_type = link_exe_rule;
    unresolved_package_name package{"com.Microsoft.VisualStudio.VC.link"s};
};
struct gcc {
    using rule_type = gcc_link_rule;
    unresolved_package_name package{"org.gnu.gcc"s};
};
struct gpp {
    using rule_type = gcc_link_rule;
    unresolved_package_name package{"org.gnu.g++"s};
};

} // namespace linker

} // namespace sw
