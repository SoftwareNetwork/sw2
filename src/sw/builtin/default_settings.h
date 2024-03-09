#pragma once

#include "build_settings.h"
#include "detect.h"

namespace sw {

auto &get_msvc_detector() {
    static msvc_detector d;
    return d;
}
void get_msvc_detector(auto &&s) {
    get_msvc_detector().add(s);
    // return d;
}

auto default_host_settings() {
    build_settings bs;
    bs.build_type = build_type::release{};
    bs.library_type = library_type::shared{};
    //bs.c.runtime = library_type::shared{};
    //bs.cpp.runtime = library_type::shared{};

    bs.arch = current_arch();

    // see more definitions at https://opensource.apple.com/source/WTF/WTF-7601.1.46.42/wtf/Platform.h.auto.html
// #if defined(__MINGW32__)
// bs.os = os::mingw{};
#if defined(_WIN32)
    bs.os = os::windows{};
    if (get_msvc_detector().exists()) {
        // mingw could use its own windows headers
        bs.forced_dependencies.emplace_back(dependency_base{.name = "com.Microsoft.Windows.SDK.um"s, .bs = bs});
        //
        bs.forced_dependencies.emplace_back(dependency_base{.name = "com.Microsoft.Windows.SDK.ucrt"s, .bs = bs});
        bs.forced_dependencies.emplace_back(dependency_base{.name = "com.Microsoft.VisualStudio.VC.libc"s, .bs = bs});
        bs.forced_dependencies.emplace_back(dependency_base{.name = "com.Microsoft.VisualStudio.VC.libcpp"s, .bs = bs});
        //bs.forced_dependencies.emplace_back(dependency_base{.name = "com.Microsoft.VisualStudio.VC.cl"s, .bs = bs});
        //bs.forced_dependencies.emplace_back(dependency_base{.name = "com.Microsoft.VisualStudio.VC.lib"s, .bs = bs});
        //bs.forced_dependencies.emplace_back(dependency_base{.name = "com.Microsoft.VisualStudio.VC.link"s, .bs = bs});

        bs.env.aliases["cl"] = "cl"s;
        bs.env.aliases["lib"] = "lib"s;
        bs.env.aliases["link"] = "link"s;
        bs.env.aliases["rc"] = "rc"s;

        bs.env.aliases["cc"] = "cl"s;
        bs.env.aliases["c++"] = "cl"s;
        bs.env.aliases["ar"] = "lib"s;
        bs.env.aliases["ld"] = "link"s;

        /*bs.c.compiler = c_compiler::msvc{}; // switch to gcc-12+
        bs.c.stdlib.emplace_back("com.Microsoft.Windows.SDK.ucrt"s);
        bs.c.stdlib.emplace_back("com.Microsoft.VisualStudio.VC.libc"s);
        bs.cpp.compiler = cpp_compiler::msvc{}; // switch to gcc-12+
        bs.cpp.stdlib.emplace_back("com.Microsoft.VisualStudio.VC.libcpp"s);
        bs.librarian = librarian::msvc{}; // switch to gcc-12+
        bs.linker = linker::msvc{};       // switch to gcc-12+
        */
    } else {
        SW_UNIMPLEMENTED;
    }
#elif defined(__APPLE__)
    bs.os = os::macos{};
    bs.c.compiler = c_compiler::gcc{};
    bs.cpp.compiler = cpp_compiler::gcc{};
    bs.librarian = librarian::ar{};
    bs.linker = linker::gpp{};
#elif defined(__linux__)
    bs.os = os::linux{};
    bs.c.compiler = c_compiler::gcc{};
    bs.cpp.compiler = cpp_compiler::gcc{};
    bs.librarian = librarian::ar{};
    bs.linker = linker::gpp{};
#else
#error "unknown os"
#endif

    return bs;
}

auto default_build_settings() {
    return default_host_settings();
}

auto make_solution() {
    auto binary_dir = ".sw";
    solution s{binary_dir, default_host_settings()};
#ifdef _WIN32
    if (get_msvc_detector().exists()) {
        get_msvc_detector(s);
        detect_winsdk(s);
    }
#endif
    // we detect clang cl on windows atleast
    detect_gcc_clang(s);
    return s;
}

} // namespace sw
