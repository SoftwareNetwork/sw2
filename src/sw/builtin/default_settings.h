#pragma once

#include "build_settings.h"
//#include "detect.h"

namespace sw {

/*auto &get_msvc_detector() {
    static msvc_detector d;
    return d;
}
void get_msvc_detector(auto &&s) {
    get_msvc_detector().add(s);
    // return d;
}*/

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
    /*if (get_msvc_detector().exists()) {
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

        //bs.c.compiler = c_compiler::msvc{}; // switch to gcc-12+
        //bs.c.stdlib.emplace_back("com.Microsoft.Windows.SDK.ucrt"s);
        //bs.c.stdlib.emplace_back("com.Microsoft.VisualStudio.VC.libc"s);
        //bs.cpp.compiler = cpp_compiler::msvc{}; // switch to gcc-12+
        //bs.cpp.stdlib.emplace_back("com.Microsoft.VisualStudio.VC.libcpp"s);
        //bs.librarian = librarian::msvc{}; // switch to gcc-12+
        //bs.linker = linker::msvc{};       // switch to gcc-12+
    } else {
        SW_UNIMPLEMENTED;
    }*/
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

/*auto make_solution() {
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
}*/

auto make_settings(auto &b) {
    std::vector<build_settings> settings{default_build_settings()};
    //
    auto static_shared_product = [&](auto &&v1, auto &v2, auto &&f) {
        if (v1 || v2) {
            if (v1 && v2) {
                for (auto &&s : settings) {
                    f(s, library_type::static_{});
                }
                auto s2 = settings;
                for (auto &&s : s2) {
                    f(s, library_type::shared{});
                    settings.push_back(s);
                }
            } else {
                if (v1) {
                    for (auto &&s : settings) {
                        f(s, library_type::static_{});
                    }
                }
                if (v2) {
                    for (auto &&s : settings) {
                        f(s, library_type::shared{});
                    }
                }
            }
        }
    };
    static_shared_product(b.static_, b.shared, [](auto &&s, auto &&v) {
        s.library_type = v;
    });
    //
    /*if (b.c_static_runtime) {
        for (auto &&s : settings) {
            s.c.runtime = library_type::static_{};
        }
    }
    if (b.cpp_static_runtime) {
        for (auto &&s : settings) {
            s.cpp.runtime = library_type::static_{};
        }
    }
    static_shared_product(b.c_and_cpp_static_runtime, b.c_and_cpp_dynamic_runtime, [](auto &&s, auto &&v) {
        s.c.runtime = v;
        s.cpp.runtime = v;
    });*/
    /*static_shared_product(b.c_and_cpp_static_runtime, b.c_and_cpp_dynamic_runtime, [](auto &&s, auto &&v) {
    * SWUNIMPLe
        s.cpp.runtime = v;
    });*/

    //
    auto cfg_product = [&](auto &&v1, auto &&f) {
        if (!v1) {
            return;
        }
        auto values = (string)v1;
        auto r = values | std::views::split(',') | std::views::transform([](auto &&word) {
                        return std::string_view{word.begin(), word.end()};
                    });
        auto s2 = settings;
        for (int i = 0; auto &&value : r) {
            bool set{};
            if (i++) {
                for (auto &&s : s2) {
                    set |= f(s, value);
                    settings.push_back(s);
                }
            } else {
                for (auto &&s : settings) {
                    set |= f(s, value);
                }
            }
            if (!set) {
                string s(value.begin(), value.end());
                throw std::runtime_error{"unknown value: "s + s};
            }
        }
    };
    auto check_and_set = [](auto &&s, auto &&v) {
        auto v2 = v.substr(0, v.find("-"));
        bool set{};
        s.for_each([&](auto &&a) {
            using T = std::decay_t<decltype(a)>;
            if (T::is(v2)) {
                T t{};
                if (v != v2) {
                    if constexpr (requires {t.package;}) {
                        t.package.range = v.substr(v.find("-") + 1);
                    }
                }
                s = t;
                set = true;
            }
        });
        return set;
    };
    cfg_product(b.arch, [&](auto &&s, auto &&v) {
        return check_and_set(s.arch, v);
    });
    cfg_product(b.config, [&](auto &&s, auto &&v) {
        return check_and_set(s.build_type, v);
    });
    /*cfg_product(b.compiler, [&](auto &&s, auto &&v) {
        return 1
        && check_and_set(s.c.compiler, v)
        && check_and_set(s.cpp.compiler, v)
        //&& check_and_set(s.linker, v)?
        ;
    });*/
    cfg_product(b.os, [&](auto &&s, auto &&v) {
        return check_and_set(s.os, v);
    });
    //
    return settings;
}

} // namespace sw
