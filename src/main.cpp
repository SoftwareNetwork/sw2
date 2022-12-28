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
            if (b.static_ || b.shared) {
                if (b.static_ && b.shared) {
                    for (auto &&s : settings) {
                        s.library_type = library_type::static_{};
                    }
                    auto s2 = settings;
                    for (auto &&s : s2) {
                        s.library_type = library_type::shared{};
                        settings.push_back(s);
                    }
                } else {
                    if (b.static_) {
                        for (auto &&s : settings) {
                            s.library_type = library_type::static_{};
                        }
                    }
                    if (b.shared) {
                        for (auto &&s : settings) {
                            s.library_type = library_type::shared{};
                        }
                    }
                }
            }
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
            if (b.c_and_cpp_static_runtime) {
                for (auto &&s : settings) {
                    s.c.runtime = library_type::static_{};
                    s.cpp.runtime = library_type::static_{};
                }
            }

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

    auto this_path = fs::current_path();
    if (cl.working_directory) {
        fs::current_path(cl.working_directory);
    }
    if (cl.sw1) {
        //Sleep(15000);
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
        cpp_emitter e;
        e.include(fs::absolute(path{std::source_location::current().file_name()}.parent_path()) / "sw.h");
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
        e.include(path{std::source_location::current().file_name()}.parent_path() / "main.cpp");
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
        //dbs.build_type = build_type::debug{};
        is.settings.insert(dbs);
        s.add_input(is);
        s.build(cl);

        auto &&t = s.targets.find_first<executable>("sw");

        raw_command c;
        c.working_directory = this_path;
        c += t.executable, "-sw1";
        for (int i = 1; i < argc; ++i) {
            c += (const char *)argv[i];
        }
        c.run();
    });
    return 0;

    /*dbs.arch = arch::x64{};
    dbs.arch = arch::x86{};
    dbs.arch = arch::arm64{};
    dbs.arch = arch::arm{};

    dbs.build_type = build_type::debug{};
    dbs.build_type = build_type::release{};*/
}
