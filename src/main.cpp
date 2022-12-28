// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2022 Egor Pugin <egor.pugin@gmail.com>

#include "sw.h"
#include "command_line.h"
#include "main.h"

void lower_drive_letter(string &s) {
    if (!s.empty()) {
        s[0] = tolower(s[0]);
    }
}
void mingw_drive_letter(string &s) {
    if (s.size() > 1 && s[1] == ':') {
        s[0] = tolower(s[0]);
        s = "/"s + s[0] + s.substr(2);
    }
}
auto normalize_path(const path &p) {
    auto fn = p.string();
    std::replace(fn.begin(), fn.end(), '\\', '/');
    return fn;
}
auto normalize_path_and_drive(const path &p) {
    auto fn = normalize_path(p);
    lower_drive_letter(fn);
    return fn;
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
            solution s;
            auto f = [&](auto &&add_input) {
#ifdef SW1_BUILD
                sw1_load_inputs(add_input);
#else
                throw std::runtime_error{"no entry function was specified"};
#endif
            };
            std::vector<build_settings> settings{default_build_settings()};
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

            auto cfg_product = [&](auto &&v1, auto &&f2) {
                if (!v1) {
                    return;
                }
                auto values = (string)v1;
                auto r = values | std::views::split(',') | std::views::transform([](auto &&word) {
                             return std::string_view{word.begin(), word.end()};
                         });
                auto s2 = settings;
                for (int i = 0; auto &&value : r) {
                    if (i++) {
                        for (auto &&s : s2) {
                            auto &v2 = s.*f2;
                            v2.for_each([&](auto &&a) {
                                using T = std::decay_t<decltype(a)>;
                                if (T::is(value)) {
                                    v2 = T{};
                                }
                            });
                            settings.push_back(s);
                        }
                    } else {
                        for (auto &&s : settings) {
                            auto &v2 = s.*f2;
                            v2.for_each([&](auto &&a) {
                                using T = std::decay_t<decltype(a)>;
                                if (T::is(value)) {
                                    v2 = T{};
                                }
                            });
                        }
                    }
                }
            };
            cfg_product(b.arch, &build_settings::arch);
            cfg_product(b.config, &build_settings::build_type);

            auto add_input = [&](auto &&f, auto &&dir) {
                input_with_settings is;
                is.i = entry_point{f, dir};
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
    }

    auto this_path = fs::current_path();
    if (cl.working_directory) {
        fs::current_path(cl.working_directory);
    }
    if (cl.sw1) {
        if (cl.int3) {
            debug_break();
        }
        sw1(cl);
    }
#ifdef SW1_BUILD
    return 0;
#endif
    visit_any(cl.c, [&](command_line_parser::build &b) {
        solution s;
        auto cfg_dir = s.binary_dir / "cfg";
        s.binary_dir = cfg_dir;
        auto fn = cfg_dir / "src" / "main.cpp";
        fs::create_directories(fn.parent_path());
        auto swdir = fs::absolute(path{std::source_location::current().file_name()}.parent_path());
        cpp_emitter e;
        e.include(swdir / "sw.h");
        e += "";
        struct spec_data {
            path fn;
            string ns;
        };
        std::vector<spec_data> nses;
        visit_any(b.i, [&](specification_file_input &i) {
            auto fn = fs::absolute(i.fn);
            auto fns = normalize_path_and_drive(fn);
            auto fnh = std::hash<string>{}(fns);
            auto nsname = "sw_ns_" + std::to_string(fnh);
            nses.push_back({fns,nsname});
            auto ns = e.namespace_(nsname);
            // add inline ns?
            e.include(fn);
        });
        e += "";
        e += "#define SW1_BUILD";
        e += "void sw1_load_inputs(auto &&f);";
        e += "";
        e.include(swdir / "main.cpp");
        e += "";
        e += "void sw1_load_inputs(auto &&f) {";
        for (auto &&ns : nses) {
            e += "    f(&" + ns.ns + "::build, \"" + ns.fn.parent_path().string() + "\");";
        }
        e += "}";
        write_file_if_different(fn, e.s);

        //fs::current_path(cfg_dir);
        s.source_dir = cfg_dir;
        input_with_settings is{entry_point{&self_build::build}};
        auto dbs = default_build_settings();
        dbs.build_type = build_type::debug{};
        is.settings.insert(dbs);
        s.add_input(is);
        s.build(cl);

        auto &&t = s.targets.find_first<executable>("sw");

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
