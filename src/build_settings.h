// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2022 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include "detect.h"

namespace sw {

auto default_host_settings() {
    build_settings bs;
    bs.build_type = build_type::release{};
    bs.library_type = library_type::shared{};
    bs.c.runtime = library_type::shared{};
    bs.cpp.runtime = library_type::shared{};

#if defined(__x86_64__) || defined(_M_X64)
    bs.arch = arch::x64{};
#elif defined(__i386__) || defined(_M_IX86)
    bs.arch = arch::x86{};
#elif defined(__arm64__) || defined(__aarch64__) || defined(_M_ARM64)
    bs.arch = arch::arm64{};
#elif defined(__arm__) || defined(_M_ARM)
    bs.arch = arch::arm{};
#else
#error "unknown arch"
#endif

    // see more definitions at https://opensource.apple.com/source/WTF/WTF-7601.1.46.42/wtf/Platform.h.auto.html
#if defined(_WIN32)
    bs.os = os::windows{};
    bs.kernel_lib = unresolved_package_name{"com.Microsoft.Windows.SDK.um"s};
    if (get_msvc_detector().exists()) {
        bs.c.compiler = c_compiler::msvc{}; // switch to gcc-12+
        bs.c.stdlib.emplace_back("com.Microsoft.Windows.SDK.ucrt"s);
        bs.c.stdlib.emplace_back("com.Microsoft.VisualStudio.VC.libc"s);
        bs.cpp.compiler = cpp_compiler::msvc{}; // switch to gcc-12+
        bs.cpp.stdlib.emplace_back("com.Microsoft.VisualStudio.VC.libcpp"s);
        bs.librarian = librarian::msvc{}; // switch to gcc-12+
        bs.linker = linker::msvc{}; // switch to gcc-12+
    } else {
        SW_UNIMPLEMENTED;
    }
#elif defined(__APPLE__)
    bs.os = os::macos{};
    bs.c.compiler = c_compiler::gcc{}; // gcc-12+
    bs.cpp.compiler = cpp_compiler::gcc{}; // gcc-12+
    bs.librarian = librarian::ar{}; // switch to gcc-12+
    bs.linker = linker::gcc{}; // switch to gcc-12+
#elif defined(__linux__)
    bs.os = os::linux{};
    // bs.c_compiler = c_compiler::gcc{};
    // bs.cpp_compiler = cpp_compiler::gcc{};
#else
#error "unknown os"
#endif

    return bs;
}

auto default_build_settings() {
    return default_host_settings();
}

} // namespace sw
