// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2022 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include "os_base.h"
#include "../package.h"

namespace sw {

struct compiler_base {
    unresolved_package_name package;

    compiler_base(const unresolved_package_name &name) : package{name} {
    }
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

} // namespace asm_compiler

namespace c_compiler {

struct clang : clang_base {
    // using rule_type = gcc_compile_rule;
    static inline constexpr auto package_name = "org.llvm.clang";

    clang() : clang_base{unresolved_package_name{package_name, "*"}} {
    }
};
struct clang_cl : clang_base {
    // using rule_type = gcc_compile_rule;
    static inline constexpr auto package_name = "org.llvm.clangcl";

    clang_cl() : clang_base{unresolved_package_name{package_name, "*"}} {
    }
};
struct gcc : gcc_base {
    // using rule_type = gcc_compile_rule;
    static inline constexpr auto package_name = "org.gnu.gcc";

    gcc() : gcc_base{unresolved_package_name{package_name, "*"}} {
    }
};
struct msvc : msvc_base {
    using msvc_base::msvc_base;
    // using rule_type = cl_exe_rule;
};

} // namespace c_compiler

namespace cpp_compiler {

struct clang : clang_base {
    // using rule_type = gcc_compile_rule;
    static inline constexpr auto package_name = "org.llvm.clang++";

    clang() : clang_base{unresolved_package_name{package_name, "*"}} {
    }
};
struct clang_cl : clang_base {
    // using rule_type = gcc_compile_rule;
    static inline constexpr auto package_name = "org.llvm.clangcl";

    clang_cl() : clang_base{unresolved_package_name{package_name, "*"}} {
    }
};
struct gcc : gcc_base {
    // using rule_type = gcc_compile_rule;
    static inline constexpr auto package_name = "org.gnu.g++";

    gcc() : gcc_base{unresolved_package_name{package_name, "*"}} {
    }
};
struct msvc : msvc_base {
    using msvc_base::msvc_base;
    // using rule_type = cl_exe_rule;
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
    // using rule_type = lib_exe_rule;
    unresolved_package_name package{"com.Microsoft.VisualStudio.VC.lib"s};
};
struct ar {
    // using rule_type = lib_ar_rule;
    unresolved_package_name package{"org.gnu.binutils.ar"s};
};

} // namespace librarian

namespace linker {

struct msvc {
    // using rule_type = link_exe_rule;
    unresolved_package_name package{"com.Microsoft.VisualStudio.VC.link"s};
};
struct gcc {
    // using rule_type = gcc_link_rule;
    unresolved_package_name package{"org.gnu.gcc"s};
};
struct gpp {
    // using rule_type = gcc_link_rule;
    unresolved_package_name package{"org.gnu.g++"s};
};

} // namespace linker

} // namespace sw
