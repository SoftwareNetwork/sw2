#pragma once

#include "helpers.h"
#include "detect.h"

namespace sw {

struct rule_flag {
    std::set<void *> rules;

    template <typename T>
    auto get_rule_tag() {
        static void *p;
        return (void *)&p;
    }
    template <typename T>
    bool contains(T &&) {
        return rules.contains(get_rule_tag<std::remove_cvref_t<std::remove_pointer_t<T>>>());
    }
    template <typename T>
    auto insert(T &&) {
        return rules.insert(get_rule_tag<std::remove_cvref_t<std::remove_pointer_t<T>>>());
    }
};

auto is_cpp_file(const path &fn) {
    static std::set<string> exts{".cpp", ".cxx"};
    return exts.contains(fn.extension().string());
}

struct sources_rule {
    void operator()(auto &tgt) const {
        for (auto &&f : tgt) {
            if (is_cpp_file(f)) {
                tgt.processed_files[f].insert(this);
            }
        }
    }
};
#ifdef _WIN32
struct cl_exe_rule {
    msvc_instance msvc;
    win_sdk_info sdk;

    cl_exe_rule() {
        msvc = detect_msvc().at(0);
        sdk = detect_winsdk();
    }

    void operator()(auto &tgt) const {
        auto compiler = msvc.cl_target();
        for (auto &&[f, rules] : tgt.processed_files) {
            if (is_cpp_file(f) && !rules.contains(this)) {
                auto out = f.filename() += ".obj";
                cl_exe_command c;
                c += compiler.exe, "-nologo", "-c", "-std:c++latest", "-EHsc", f, "-Fo" + out.string();
                auto add = [&](auto &&tgt) {
                    for (auto &&d : tgt.definitions) {
                        c += (string)d;
                    }
                    for (auto &&i : tgt.include_directories) {
                        c += "-I" + i.string();
                    }
                };
                add(tgt.compile_options);
                add(msvc.stdlib_target());
                add(sdk.ucrt);
                add(sdk.um);
                c.inputs.insert(f);
                c.outputs.insert(out);
                tgt.commands.emplace_back(std::move(c));
                rules.insert(this);
            }
        }
    }
};
struct link_exe_rule {
    msvc_instance msvc;
    win_sdk_info sdk;

    link_exe_rule() {
        msvc = detect_msvc().at(0);
        sdk = detect_winsdk();
    }

    void operator()(auto &tgt) const {
        path out = (string)tgt.name + ".exe";
        auto linker = msvc.link_target();
        io_command c;
        c += linker.exe, "-nologo", "-OUT:" + out.string();
        for (auto &&[f, rules] : tgt.processed_files) {
            if (f.extension() == ".obj") {
                c += f;
                c.inputs.insert(f);
                rules.insert(this);
            }
        }
        auto add = [&](auto &&tgt) {
            for (auto &&i : tgt.link_directories) {
                c += "-LIBPATH:" + i.string();
            }
            for (auto &&d : tgt.link_libraries) {
                c += d.string();
            }
        };
        add(tgt.link_options);
        add(msvc.stdlib_target());
        add(sdk.ucrt);
        add(sdk.um);
        c.outputs.insert(out);
        tgt.commands.emplace_back(std::move(c));
    }
};
#endif

using rule_types = types<sources_rule
#ifdef _WIN32
    , cl_exe_rule, link_exe_rule
#endif
>;
using rule = decltype(make_variant(rule_types{}))::type;

}
