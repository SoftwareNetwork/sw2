// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2022 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include "rule_list.h"
#include "target_list.h"

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

auto is_c_file(const path &fn) {
    static std::set<string> exts{".c", ".m"}; // with obj-c, separate call?
    return exts.contains(fn.extension().string());
}
auto is_cpp_file(const path &fn) {
    static std::set<string> exts{".cpp", ".cxx", ".mm"}; // with obj-c++, separate call?
    return exts.contains(fn.extension().string());
}

struct native_sources_rule {
    void operator()(auto &tgt) const {
        for (auto &&f : tgt) {
            if (is_cpp_file(f) || is_c_file(f)) {
                tgt.processed_files[f].insert(this);
            }
        }
    }
};

string format_log_record(auto &&tgt, auto &&second_part) {
    SW_UNIMPLEMENTED;
    /*string s = std::format("[{}]", (string)tgt.name);
    string cfg = "/[";
    tgt.bs.arch.visit([&](auto &&a) {
        cfg += std::format("{},", std::decay_t<decltype(a)>::name);
    });
    tgt.bs.library_type.visit([&](auto &&a) {
        cfg += std::format("{},", std::decay_t<decltype(a)>::name); // short?
    });
    tgt.bs.build_type.visit_no_special([&](auto &&a) {
        cfg += std::format("{},", std::decay_t<decltype(a)>::short_name);
    });
    if (tgt.bs.cpp_static_runtime) {
        cfg += "cppmt,";
    }
    if (tgt.bs.c_static_runtime) {
        cfg += "cmt,";
    }
    cfg.resize(cfg.size() - 1);
    cfg += "]";
    s += cfg + second_part;
    return s;*/
}

struct cl_exe_rule {
    using target_type = binary_target_msvc;

    target_type &compiler;

    cl_exe_rule(target_uptr &t) : compiler{*std::get<uptr<target_type>>(t)} {}

    void operator()(auto &tgt) requires requires { tgt.compile_options; } {
        for (auto &&[f, rules] : tgt.processed_files) {
            if (rules.contains(this) || !(is_c_file(f) || is_cpp_file(f))) {
                continue;
            }
            cl_exe_command c;
            c.working_directory = tgt.binary_dir / "obj";
            auto out = tgt.binary_dir / "obj" / f.filename() += ".obj";
            c.name_ = format_log_record(tgt, "/"s + normalize_path(f.lexically_relative(tgt.source_dir).string()));
            c.old_includes = compiler.msvc.vs_version < package_version{16,7};
            c += compiler.executable, "-nologo", "-c";
            c.inputs.insert(compiler.executable);
            c += "-FS"; // ForceSynchronousPDBWrites
            c += "-Zi"; // DebugInformationFormatType::ProgramDatabase
            tgt.bs.build_type.visit_any(
                [&](build_type::debug) {
                    c += "-Od";
                }, [&](build_type::release) {
                    c += "-O2";
                });
            if (is_c_file(f)) {
                if (tgt.bs.c_static_runtime) {
                    tgt.bs.build_type.visit_any(
                        [&](build_type::debug) {
                            c += "-MTd";
                        },
                        [&](build_type::release) {
                            c += "-MT";
                        });
                } else {
                    tgt.bs.build_type.visit_any(
                        [&](build_type::debug) {
                            c += "-MDd";
                        },
                        [&](build_type::release) {
                            c += "-MD";
                        });
                }
            }
            if (is_cpp_file(f)) {
                c += "-EHsc"; // enable for c too?
                c += "-std:c++latest";

                if (tgt.bs.cpp_static_runtime) {
                    tgt.bs.build_type.visit_any(
                        [&](build_type::debug) {
                            c += "-MTd";
                        },
                        [&](build_type::release) {
                            c += "-MT";
                        });
                } else {
                    tgt.bs.build_type.visit_any(
                        [&](build_type::debug) {
                            c += "-MDd";
                        },
                        [&](build_type::release) {
                            c += "-MD";
                        });
                }
            }
            c += f, "-Fo" + out.string();
            auto add = [&](auto &&tgt) {
                for (auto &&d : tgt.definitions) {
                    c += (string)d;
                }
                for (auto &&i : tgt.include_directories) {
                    c += "-I", i;
                }
            };
            add(tgt.compile_options);
            for (auto &&d : tgt.dependencies) {
                visit(*d.target, [&](auto &&v) {
                    if constexpr (requires { v->definitions; }) {
                        add(*v);
                    }
                });
            }
            c.inputs.insert(f);
            c.outputs.insert(out);
            tgt.commands.emplace_back(std::move(c));
            rules.insert(this);
        }
    }
};
struct lib_exe_rule {
    using target_type = binary_target;

    target_type &librarian;

    lib_exe_rule(target_uptr &t) : librarian{*std::get<uptr<target_type>>(t)} {
    }

    void operator()(auto &tgt) requires requires { tgt.library; } {
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
    using target_type = binary_target;

    target_type &linker;

    link_exe_rule(target_uptr &t) : linker{*std::get<uptr<target_type>>(t)} {}

    void operator()(auto &tgt) requires requires { tgt.link_options; } {
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
            if (f.extension() == ".obj") {
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
        auto add = [&](auto &&tgt) {
            for (auto &&i : tgt.link_directories) {
                c += "-LIBPATH:" + i.string();
            }
            for (auto &&d : tgt.link_libraries) {
                c += d;
            }
            for (auto &&d : tgt.system_link_libraries) {
                c += d;
            }
        };
        add(tgt.link_options);
        for (auto &&d : tgt.dependencies) {
            visit(*d.target, [&](auto &&v) {
                if constexpr (requires { v->link_directories; }) {
                    add(*v);
                }
            });
        }
        tgt.commands.emplace_back(std::move(c));
    }
};

struct gcc_instance {
    path bin;

    auto cl_target() const {
        /*binary_target t{package_name{"org.gnu.gcc"s, "0.0.1"}};
        t.executable = bin;
        return t;*/
    }
    auto link_target() const {
        /*binary_target t{package_name{"org.gnu.gcc"s, "0.0.1"}};
        t.executable = bin;
        return t;*/
    }
};

auto detect_gcc_clang() {
    std::vector<gcc_instance> cls;
    cls.emplace_back("/usr/bin/g++"s);
    return cls;
}

struct gcc_compile_rule {
    gcc_instance gcc;

    gcc_compile_rule() {
        //gcc = detect_gcc_clang().at(0);
    }

    void operator()(auto &tgt) requires requires { tgt.compile_options; } {
        /*auto compiler = gcc.cl_target();
        for (auto &&[f, rules] : tgt.processed_files) {
            if (is_cpp_file(f) && !rules.contains(this)) {
                auto out = tgt.binary_dir / "obj" / f.filename() += ".o";
                gcc_command c;
                c += compiler.executable, "-c", "-std=c++2b", f, "-o", out;
                auto add = [&](auto &&tgt) {
                    for (auto &&d : tgt.definitions) {
                        c += (string)d;
                    }
                    for (auto &&i : tgt.include_directories) {
                        c += "-I", i;
                    }
                };
                add(tgt.compile_options);
                c.inputs.insert(f);
                c.outputs.insert(out);
                tgt.commands.emplace_back(std::move(c));
                rules.insert(this);
            }
        }*/
    }
};
struct gcc_link_rule {
    gcc_instance gcc;

    gcc_link_rule() {
        gcc = detect_gcc_clang().at(0);
    }

    void operator()(auto &tgt) requires requires { tgt.link_options; } {
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
        auto add = [&](auto &&tgt) {
            for (auto &&i : tgt.link_directories) {
                c += "-L", i;
            }
            for (auto &&d : tgt.link_libraries) {
                c += d;
            }
            for (auto &&d : tgt.system_link_libraries) {
                c += d;
            }
        };
        add(tgt.link_options);
        c.outputs.insert(out);
        tgt.commands.emplace_back(std::move(c));*/
    }
};

using rule = rule_types::variant_type;

} // namespace sw
