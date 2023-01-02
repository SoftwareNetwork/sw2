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

struct cl_exe_rule {
    //executable_target &compiler;
    //cl_exe_rule(executable_target &t) : compiler{t} {}

    void operator()(auto &tgt, auto &compiler) requires requires { tgt.compile_options; } {
        auto objext = tgt.bs.os.visit([](auto &&v) -> string_view {
            if constexpr (requires { v.object_file_extension; }) {
                return v.object_file_extension;
            } else {
                throw std::runtime_error{"no object extension"};
            }
        });

        for (auto &&[f, rules] : tgt.processed_files) {
            if (rules.contains(this) || !(is_c_file(f) || is_cpp_file(f))) {
                continue;
            }
            cl_exe_command c;
            c.working_directory = tgt.binary_dir / "obj";
            auto out = tgt.binary_dir / "obj" / f.filename() += objext;
            c.name_ = format_log_record(tgt, "/"s + normalize_path(f.lexically_relative(tgt.source_dir).string()));
            c.old_includes = compiler.msvc.vs_version < package_version{16,7};
            c += compiler.executable, "-nologo", "-c";
            c.inputs.insert(compiler.executable);
            c += "-FS"; // ForceSynchronousPDBWrites
            c += "-Zi"; // DebugInformationFormatType::ProgramDatabase
            tgt.bs.build_type.visit(
                [&](build_type::debug) {
                    c += "-Od";
                }, [&](auto) {
                    c += "-O2";
                });
            auto mt_md = [&](auto &&obj) {
                if (obj.template is<library_type::static_>()) {
                    tgt.bs.build_type.visit(
                        [&](build_type::debug) {
                            c += "-MTd";
                        },
                        [&](auto) {
                            c += "-MT";
                        });
                } else {
                    tgt.bs.build_type.visit(
                        [&](build_type::debug) {
                            c += "-MDd";
                        },
                        [&](auto) {
                            c += "-MD";
                        });
                }
            };
            if (is_c_file(f)) {
                mt_md(tgt.bs.c.runtime);
            } else if (is_cpp_file(f)) {
                c += "-EHsc"; // enable for c too?
                c += "-std:c++latest";
                mt_md(tgt.bs.cpp.runtime);
            }
            c += f, "-Fo" + out.string();
            add_compile_options(tgt.merge_object(), c);
            c.inputs.insert(f);
            c.outputs.insert(out);
            tgt.commands.emplace_back(std::move(c));
            rules.insert(this);
        }
    }
};
struct lib_exe_rule {
    //using target_type = binary_target_msvc;

    //target_type &librarian;

    //lib_exe_rule(target_uptr &t) : librarian{*std::get<uptr<target_type>>(t)} {}

    void operator()(auto &tgt, auto &librarian) requires requires { tgt.library; } {
        io_command c;
        c.err = ""s;
        c.out = ""s;
        c += librarian.executable, "-nologo";
        c.inputs.insert(librarian.executable);
        c.name_ = format_log_record(tgt, tgt.library.extension().string());
        c += "-OUT:" + tgt.library.string();
        c.outputs.insert(tgt.library);
        for (auto &&[f, rules] : tgt.processed_files) {
            if (f.extension() == ".obj") {
                c += f;
                c.inputs.insert(f);
                rules.insert(this);
            }
        }
        tgt.commands.emplace_back(std::move(c));
    }
};
struct link_exe_rule {
    //using target_type = binary_target_msvc;

    //target_type &linker;

    //link_exe_rule(target_uptr &t) : linker{*std::get<uptr<target_type>>(t)} {}

    void operator()(auto &tgt, auto &linker) requires requires { tgt.link_options; } {
        auto objext = tgt.bs.os.visit([](auto &&v) -> string_view {
            if constexpr (requires { v.object_file_extension; }) {
                return v.object_file_extension;
            } else {
                throw std::runtime_error{"no object extension"};
            }
        });

        io_command c;
        c.err = ""s;
        c.out = ""s;
        c += linker.executable, "-nologo";
        c.inputs.insert(linker.executable);
        if constexpr (requires { tgt.executable; }) {
            c.name_ = format_log_record(tgt, tgt.executable.extension().string());
            c += "-OUT:" + tgt.executable.string();
            c.outputs.insert(tgt.executable);
        } else if constexpr (requires { tgt.library; }) {
            c.name_ = format_log_record(tgt, tgt.library.extension().string());
            if (!tgt.implib.empty()) {
                c += "-DLL";
                c += "-IMPLIB:" + tgt.implib.string();
                c.outputs.insert(tgt.implib);
            }
            c += "-OUT:" + tgt.library.string();
            c.outputs.insert(tgt.library);
        } else {
            SW_UNIMPLEMENTED;
        }
        for (auto &&[f, rules] : tgt.processed_files) {
            if (f.extension() == objext) {
                c += f;
                c.inputs.insert(f);
                rules.insert(this);
            }
        }
        c += "-NODEFAULTLIB";
        tgt.bs.build_type.visit_any(
            [&](build_type::debug) {
                c += "-DEBUG:FULL";
            },
            [&](build_type::release) {
                c += "-DEBUG:NONE";
            });
        auto add = [&](auto &&v) {
            for (auto &&i : v.link_directories) {
                c += "-LIBPATH:" + i.string();
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
