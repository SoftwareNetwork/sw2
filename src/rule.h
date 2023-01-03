// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2022 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include "os.h"

namespace sw {

string format_log_record(auto &&tgt, auto &&second_part) {
    string s = format("[{}]", (string)tgt.name);
    string cfg = "/[";
    tgt.bs.os.visit([&](auto &&a) {
        cfg += format("{},", std::decay_t<decltype(a)>::name);
    });
    tgt.bs.arch.visit([&](auto &&a) {
        cfg += format("{},", std::decay_t<decltype(a)>::name);
    });
    tgt.bs.library_type.visit([&](auto &&a) {
        cfg += format("{},", std::decay_t<decltype(a)>::name); // short?
    });
    tgt.bs.build_type.visit_no_special([&](auto &&a) {
        cfg += format("{},", std::decay_t<decltype(a)>::short_name);
    });
    if (tgt.bs.cpp.runtime.template is<library_type::static_>()) {
        cfg += "cppmt,";
    }
    if (tgt.bs.c.runtime.template is<library_type::static_>()) {
        cfg += "cmt,";
    }
    cfg.resize(cfg.size() - 1);
    cfg += "]";
    s += cfg + second_part;
    return s;
}

auto make_rule(auto &&f) {
    return [f](auto &&var) mutable {
        std::visit(
            [&](auto &&v) mutable {
                f(*v);
            },
            var);
    };
}

void add_compile_options(auto &&obj, auto &&c) {
    for (auto &&o : obj.compile_options) {
        c += o;
    }
    for (auto &&d : obj.definitions) {
        c += (string)d;
    }
    for (auto &&i : obj.include_directories) {
        c += "-I", i;
    }
}

struct gcc_compile_rule {
    //using target_type = binary_target;

    //target_type &compiler;
    bool clang{};
    bool cpp{};

    //gcc_compile_rule(target_uptr &t, bool clang = false, bool cpp = false)
        //: compiler{*std::get<uptr<target_type>>(t)}, clang{clang}, cpp{cpp} {}

    void operator()(auto &tgt, auto &compiler) requires requires { tgt.compile_options; } {
        auto objext = tgt.bs.os.visit(
            [](auto &&v) -> string_view {
                if constexpr (requires {v.object_file_extension;}) {
                    return v.object_file_extension;
                } else {
                    throw std::runtime_error{"no object extension"};
                }
            }
        );

        for (auto &&[f, rules] : tgt.processed_files) {
            if (rules.contains(this) || !(is_c_file(f) || is_cpp_file(f))) {
                continue;
            }
            if (cpp != is_cpp_file(f)) {
                continue;
            }
            auto out = tgt.binary_dir / "obj" / f.filename() += objext;
            gcc_command c;
            c.name_ = format_log_record(tgt, "/"s + normalize_path(f.lexically_relative(tgt.source_dir).string()));
            c += compiler.executable, "-c";
            if (is_c_file(f)) {
                c += "-std=c17";
            } else if (is_cpp_file(f)) {
                c += "-std=c++2b";
            }
            if (clang) {
                string t = "--target=";
                tgt.bs.arch.visit_no_special(
                    [&](auto &&v) {
                        t += v.clang_target_name;
                    });
                t += "-unknown-";
                tgt.bs.os.visit_no_special([&](auto &&v) {
                    t += v.name;
                });
                c += t;
            }
            c += f, "-o", out;
            add_compile_options(tgt.merge_object(), c);
            c.inputs.insert(f);
            c.outputs.insert(out);
            tgt.commands.emplace_back(std::move(c));
            rules.insert(this);
        }
    }
};
struct gcc_link_rule {
    //using target_type = binary_target;

    //target_type &linker;

    //gcc_link_rule(target_uptr &t) : linker{*std::get<uptr<target_type>>(t)} {}

    void operator()(auto &tgt, auto &linker) requires requires { tgt.link_options; } {
        io_command c;
        c += linker.executable;
        if constexpr (requires { tgt.executable; }) {
            c.name_ = format_log_record(tgt, "");
            c += "-o", tgt.executable.string();
            c.outputs.insert(tgt.executable);
        } else if constexpr (requires { tgt.library; }) {
            c.name_ = format_log_record(tgt, tgt.library.extension().string());
            if (!tgt.implib.empty()) {
                //mingw?cygwin?
                //c += "-DLL";
                //c += "-IMPLIB:" + tgt.implib.string();
                //c.outputs.insert(tgt.implib);
            }
            c += "-o", tgt.library.string();
            c.outputs.insert(tgt.library);
        } else {
            SW_UNIMPLEMENTED;
        }
        for (auto &&[f, rules] : tgt.processed_files) {
            if (f.extension() == ".o") {
                c += f;
                c.inputs.insert(f);
                rules.insert(this);
            }
        }
        auto add = [&](auto &&v) {
            for (auto &&i : v.link_directories) {
                c += "-L", i;
            }
            for (auto &&d : v.link_libraries) {
                c += d;
            }
            for (auto &&d : v.system_link_libraries) {
                c += d;
            }
        };
        add(tgt.merge_object());
        tgt.commands.emplace_back(std::move(c));
    }
};
struct lib_ar_rule {
    //using target_type = binary_target;

    //target_type &compiler;

    //lib_ar_rule(target_uptr &t) : compiler{*std::get<uptr<target_type>>(t)} {}

    void operator()(auto &tgt, auto &ar) requires requires { tgt.link_options; } {
        int a = 5;
        a++;
        SW_UNIMPLEMENTED;

        /*path out = tgt.binary_dir / "bin" / (string)tgt.name;
        auto linker = gcc.link_target();
        io_command c;
        c += linker.executable, "-o", out.string();
        for (auto &&[f, rules] : tgt.processed_files) {
            if (f.extension() == ".o") {
                c += f;
                c.inputs.insert(f);
                rules.insert(this);
            }
        }
        auto add = [&](auto &&v) {
            for (auto &&i : v.link_directories) {
                c += "-L", i;
            }
            for (auto &&d : v.link_libraries) {
                c += d;
            }
            for (auto &&d : v.system_link_libraries) {
                c += d;
            }
        };
        add(tgt.merge_object().link_options);
        c.outputs.insert(out);
        tgt.commands.emplace_back(std::move(c));*/
    }
};

//using rule = rule_types::variant_type;

} // namespace sw
