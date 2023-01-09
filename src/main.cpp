// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2022 Egor Pugin <egor.pugin@gmail.com>

#include "sw.h"
#include "command_line.h"
#include "builtin/detect.h"
#include "main.h"

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
        bs.linker = linker::msvc{};       // switch to gcc-12+
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
    // detect_gcc_clang(s);
#else
    detect_gcc_clang(s);
#endif
    return s;
}

struct cpp_emitter {
    struct ns {
        cpp_emitter &e;
        ns(cpp_emitter &e, auto &&name) : e{e} {
            e += "namespace "s + name + " {";
        }
        ~ns() {
            e += "}";
        }
    };

    string s;
    int indent{};

    cpp_emitter &operator+=(auto &&s) {
        add_line(s);
        return *this;
    }
    void add_line(auto &&s) {
        this->s += s + "\n"s;
    }
    void include(const path &p) {
        auto fn = normalize_path_and_drive(p);
        s += "#include \"" + fn + "\"\n";
    }
    auto namespace_(auto &&name) {
        return ns{*this, name};
    }
};

void sw1(auto &cl) {
    visit(
        cl.c,
        [&](command_line_parser::build &b) {
            auto s = make_solution();
            auto f = [&](auto &&in) {
#ifdef SW1_BUILD
                sw1_load_inputs(in);
#else
                throw std::runtime_error{"no entry function was specified"};
#endif
            };

            std::vector<build_settings> settings{default_build_settings()};
            {
                swap_and_restore sr{s.dry_run, true};
                auto add_input = [&](auto &&f, auto &&dir) {
                    input_with_settings is;
                    is.ep = entry_point{f, dir};
                    is.settings.insert(settings.begin(), settings.end());
                    s.gather_entry_points(is);
                };
                f(add_input);
            }

            auto apply = [&]() {
            };
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
            if (b.c_static_runtime) {
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
            });
            /*static_shared_product(b.c_and_cpp_static_runtime, b.c_and_cpp_dynamic_runtime, [](auto &&s, auto &&v) {
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
                bool set{};
                s.for_each([&](auto &&a) {
                    using T = std::decay_t<decltype(a)>;
                    if (T::is(v)) {
                        s = T{};
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
            cfg_product(b.compiler, [&](auto &&s, auto &&v) {
                return 1
                && check_and_set(s.c.compiler, v)
                && check_and_set(s.cpp.compiler, v)
                //&& check_and_set(s.linker, v)?
                ;
            });
            cfg_product(b.os, [&](auto &&s, auto &&v) {
                return check_and_set(s.os, v);
            });
            //

            auto add_input = [&](auto &&f, auto &&dir) {
                input_with_settings is;
                is.ep = entry_point{f, dir};
                is.settings.insert(settings.begin(), settings.end());
                s.add_input(is);
            };
            f(add_input);
            s.build(cl);
        },
        [](auto &&) {
            SW_UNIMPLEMENTED;
        });
}

int main1(int argc, char *argv[]) {
    command_line_parser cl{argc, argv};

    if (cl.sleep) {
        std::this_thread::sleep_for(std::chrono::seconds(cl.sleep));
        std::cerr << "sleep completed\n";
    }

    auto this_path = fs::current_path();
    if (cl.working_directory) {
        fs::current_path(cl.working_directory);
    }
    if (cl.sw1) {
        if (cl.int3) {
            debug_break_if_not_attached();
        }
        sw1(cl);
    }
#ifdef SW1_BUILD
    return 0;
#endif
    visit_any(cl.c, [&](command_line_parser::build &b) {
        auto s = make_solution();
        auto cfg_dir = s.binary_dir / "cfg";
        s.binary_dir = cfg_dir;
        auto fn = cfg_dir / "src" / "main.cpp";
        fs::create_directories(fn.parent_path());
        auto swdir = fs::absolute(path{std::source_location::current().file_name()}.parent_path());
        cpp_emitter e;
        e += "#define SW1_BUILD";
        e += "void sw1_load_inputs(auto &&f);";
        e.include(swdir / "main.cpp");
        e += "";
        string load_inputs;
        std::vector<input> inputs;
        if (!b.inputs) {
            b.inputs.value = std::vector<string>{"."};
        }
        for (auto &&bi : *b.inputs.value) {
            input i;
            auto check_spec = [&](auto &&fn) {
                auto p = path{bi} / fn;
                if (fs::exists(p)) {
                    i = specification_file_input{p};
                    return true;
                }
                return false;
            };
            if (0
                || check_spec("sw.h")
                || check_spec("sw.cpp") // old compat. After rewrite remove sw.h
                //|| check_spec("sw2.cpp")
                //|| (i = directory_input{"."}, true)
                ) {
                inputs.push_back(i);
            } else {
                throw std::runtime_error{"no inputs found/heuristics not implemented"};
            }
        }
        for (auto &&i : inputs) {
            visit_any(i, [&](specification_file_input &i) {
                auto fn = fs::absolute(i.fn);
                auto fns = normalize_path_and_drive(fn);
                auto fnh = std::hash<string>{}(fns);
                auto nsname = "sw_ns_" + std::to_string(fnh);
                auto ns = e.namespace_(nsname);
                e += "namespace this_namespace = ::" + nsname + ";";
                // add inline ns?
                e.include(fn);
                load_inputs += "    f(&" + nsname + "::build, \"" + normalize_path_and_drive(fn.parent_path()) + "\");\n";
            });
            e += "";
        }
        if (!load_inputs.empty()) {
            load_inputs.resize(load_inputs.size() - 1);
        }
        e += "void sw1_load_inputs(auto &&f) {";
        e += load_inputs;
        e += "}";
        write_file_if_different(fn, e.s);

        input_with_settings is{entry_point{&self_build::build,cfg_dir}};
        auto dbs = default_build_settings();
        dbs.build_type = build_type::debug{};
        is.settings.insert(dbs);
        s.add_input(is);
        s.build(cl);

        auto &&t = s.targets.find_first<executable>("sw");
        if (!fs::exists(t.executable)) {
            throw std::runtime_error{"missing sw1 file"};
        }

        auto setup_path = [](auto &&in) {
            auto s = normalize_path_and_drive(in);
            if (is_mingw_shell()) {
               //mingw_drive_letter(s);
            }
            return path{s};
        };
        raw_command c;
        c.working_directory = setup_path(this_path);
        c += setup_path(t.executable), "-sw1";
        for (int i = 1; i < argc; ++i) {
            c += (const char *)argv[i];
        }
        c.run();
    });
    return 0;
}
