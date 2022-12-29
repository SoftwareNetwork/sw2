// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2022 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include "rule.h"
#include "os.h"
#include "target_list.h"

namespace sw {

struct dependency {
    target_uptr *target;
};

// basic target: throw files, rules etc.
struct rule_target : files_target {
    using base = files_target;

#ifdef _MSC_VER
    using base::operator+=;
#endif
    using base::add;
    using base::remove;

    const build_settings solution_bs;
    build_settings bs;
    path binary_dir;
    command_storage cs;
    std::vector<rule> rules;
    std::map<path, rule_flag> processed_files;
    std::vector<command> commands;

    rule_target(auto &&solution, auto &&id)
        : files_target{id}
        , solution_bs{*solution.bs}
        , bs{*solution.bs}
   {
        source_dir = solution.source_dir;
        binary_dir = make_binary_dir(solution.binary_dir);
    }
    path make_binary_dir(const path &parent) {
        return make_binary_dir(parent, bs.hash());
    }
    path make_binary_dir(const path &parent, auto &&config) {
        return parent / "t" / std::to_string(config) / std::to_string(name.hash());
    }

#ifndef _MSC_VER
    auto operator+=(auto &&v) {
        add(v);
        return appender{[&](auto &&v) { add(v); }};
    }
    auto operator-=(auto &&v) {
        remove(v);
        return appender{[&](auto &&v) { remove(v); }};
    }
#endif

    /*auto &build_settings() {
        return bs;
    }
    auto &build_settings() const {
        return bs;
    }*/

    template <typename T, typename ... Types>
    bool is() { return bs.is<T, Types...>(); }
    void visit(auto && ... f) {
        bs.visit(FWD(f)...);
    }

    void add(const rule &r) {
        rules.push_back(r);
    }
    void init_rules(/*this */auto &&self) {
        for (auto &&r : self.rules) {
            std::visit([&](auto &&v){
                if constexpr (requires {v(self);}) {
                    v(self);
                }
            }, r);
            for (auto &&c : self.commands) {
                ::sw::visit(c, [&](auto &&c) {
                    for (auto &&o : c.outputs) {
                        self.processed_files[o];
                    }
                });
            }
        }
        self.cs.open(self.binary_dir);
        for (auto &&c : self.commands) {
            ::sw::visit(c, [&](auto &&c2) {
                c2.cs = &self.cs;
            });
        }
    }

    void prepare(/*this */auto &&self) {
        self.init_rules(self);
    }
    void build(/*this */auto &&self) {
        self.prepare(self);

        command_executor ce;
        ce.run(self);
    }

    //auto visit()
};

struct target_data {
    compile_options_t compile_options;
    link_options_t link_options;
    std::vector<dependency> dependencies;
};
struct target_data_storage : target_data {
    struct groups {
        enum {
            self    = 0b001,
            project = 0b010,
            others  = 0b100,
        };
    };

    // we need 2^3 = 8 values
    // minus 1 inherited
    // plus  1 merge_object value
    //
    // inherited = 0
    // array[0] = merge_object
    std::array<std::unique_ptr<target_data>, 8> data;

    target_data &private_{*this};
    target_data &protected_{get(groups::self | groups::project)};
    target_data &public_{get(groups::self | groups::project | groups::others)};
#undef interface // some win32 stuff
    target_data &interface{get(groups::project | groups::others)};
    target_data &interface_{get(groups::project | groups::others)}; // for similarity

protected:
    target_data &merge_object{get(0)};
private:
    target_data &get(int i) {
        if (!data[i]) {
            data[i] = std::make_unique<target_data>();
        }
        return *data[i];
    }
};

struct native_target : rule_target, target_data_storage {
    using base = rule_target;

#ifdef _MSC_VER
    using base::operator+=;
#endif
    using base::add;
    using base::remove;

    native_target(auto &&s, auto &&id) : base{s, id} {
        *this += native_sources_rule{};
        init_compilers(s);

        bs.os.visit_any([&](os::windows) {
            *this += "SW_EXPORT=__declspec(dllexport)"_def;
            *this += "SW_IMPORT=__declspec(dllimport)"_def;
        });
        if (!bs.build_type.is<build_type::debug>()) {
            *this += "NDEBUG"_def;
        }
    }

    void init_compilers(auto &&s) {
        auto load = [&] {
            std::once_flag once;
            auto load = [&] {
#ifdef _WIN32
                detect_winsdk(s);
                get_msvc_detector().add(s);
                detect_gcc_clang(s);
#else
                detect_gcc_clang(s);
#endif
            };
            visit_any(
                bs.c.compiler,
                [&](c_compiler::msvc &c) {
                    std::call_once(once, load);
                },
                [&](c_compiler::gcc &c) {
                    std::call_once(once, load);
                },
                [&](c_compiler::clang &c) {
                    std::call_once(once, load);
                });
            visit_any(
                bs.cpp.compiler,
                [&](cpp_compiler::msvc &c) {
                    std::call_once(once, load);
                },
                [&](cpp_compiler::gcc &c) {
                    std::call_once(once, load);
                },
                [&](cpp_compiler::clang &c) {
                    std::call_once(once, load);
                });
            visit_any(bs.linker, [&](librarian::msvc &c) {
                std::call_once(once, load);
            });
            visit_any(bs.linker, [&](linker::msvc &c) {
                std::call_once(once, load);
            });
        };
        std::call_once(s.system_targets_detected, load);

        // order
        for (auto &&l : bs.cpp.stdlib) {
            add(s.load_target(l, bs));
        }
        for (auto &&l : bs.c.stdlib) {
            add(s.load_target(l, bs));
        }
#if defined(_WIN32)
        add(s.load_target(bs.kernel_lib, bs));
#endif

        ::sw::visit(
            bs.c.compiler,
            [&](c_compiler::msvc &c) {
                auto &t = s.load_target(c.package, bs);
                add(c_compiler::msvc::rule_type{t});
            },
            [&](c_compiler::gcc &c) {
                auto &t = s.load_target(c.package, bs);
                add(c_compiler::gcc::rule_type{t});
            },
            [&](c_compiler::clang &c) {
                auto &t = s.load_target(c.package, bs);
                add(c_compiler::clang::rule_type{t,true});
            },
            [](auto &) {
                SW_UNIMPLEMENTED;
            });
        ::sw::visit(
            bs.cpp.compiler,
            [&](cpp_compiler::msvc &c) {
                auto &t = s.load_target(c.package, bs);
                add(cpp_compiler::msvc::rule_type{t});
            },
            [&](cpp_compiler::gcc &c) {
                auto &t = s.load_target(c.package, bs);
                add(cpp_compiler::gcc::rule_type{t,false,true});
            },
            [&](cpp_compiler::clang &c) {
                auto &t = s.load_target(c.package, bs);
                add(cpp_compiler::clang::rule_type{t,true,true});
            },
            [](auto &) {
                SW_UNIMPLEMENTED;
            });
    }

#ifndef _MSC_VER
    auto operator+=(auto &&v) {
        add(v);
        return appender{[&](auto &&v) { add(v); }};
    }
    auto operator-=(auto &&v) {
        remove(v);
        return appender{[&](auto &&v) { remove(v); }};
    }
#endif

    void add(target_uptr &ptr) {
        dependencies.push_back({&ptr});
    }
    void add(const system_link_library &l) {
        link_options.system_link_libraries.push_back(l);
    }
    void add(const definition &d) {
        compile_options.definitions.push_back(d);
    }
    void add(const include_directory &d) {
        compile_options.include_directories.push_back(d);
    }
    void add(const compile_option &d) {
        compile_options.compile_options.push_back(d);
    }

    //void build() { operator()(); }
    //void run(){}
};

struct native_library_target : native_target {
    using base = native_target;
    path library;
    path implib;

    native_library_target(auto &&s, auto &&id, std::optional<bool> shared = {}) : base{s, id} {
        if (!shared) {
            shared = bs.library_type.visit(
                [](const library_type::static_ &){
                    return false;
                },
                [](const library_type::shared &) {
                    return true;
                }, [](auto &) {
                    return true; // for now shared if the default
                }
            );
        }
        if (*shared) {
            bs.library_type = library_type::shared{};
        } else if (!*shared) {
            bs.library_type = library_type::static_{};
        }
        binary_dir = make_binary_dir(s.binary_dir);

        if (is<library_type::shared>()) {
            ::sw::visit(
                bs.linker,
                [&](linker::msvc &c) {
                    auto &t = s.load_target(c.package, bs);
                    add(linker::msvc::rule_type{t});
                },
                [](auto &) {
                    SW_UNIMPLEMENTED;
                });

            library = binary_dir / "bin" / (string)name;
            implib = binary_dir / "lib" / (string)name;
            ::sw::visit(bs.os, [&](auto &&os) {
                if constexpr (requires { os.shared_library_extension; }) {
                    library += os.shared_library_extension;
                }
                if constexpr (requires { os.shared_library_extension; }) {
                    implib += os.static_library_extension;
                }
            });
            bs.os.visit_any([&](os::windows) {
                *this += "_WINDLL"_def;
            });
        } else {
            ::sw::visit(
                bs.librarian,
                [&](librarian::msvc &c) {
                    auto &t = s.load_target(c.package, bs);
                    add(librarian::msvc::rule_type{t});
                },
                [](auto &) {
                    SW_UNIMPLEMENTED;
                });

            library = binary_dir / "lib" / (string)name;
            ::sw::visit(bs.os, [&](auto &&os) {
                if constexpr (requires { os.static_library_extension; }) {
                    library += os.static_library_extension;
                }
            });
        }
    }
};
struct native_shared_library_target : native_library_target {
    using base = native_library_target;
    native_shared_library_target(auto &&s, auto &&id) : base{s, id, true} {
    }
};
struct native_static_library_target : native_library_target {
    using base = native_library_target;
    native_static_library_target(auto &&s, auto &&id) : base{s, id, false} {
    }
};

struct executable_target : native_target {
    using base = native_target;
    path executable;

    executable_target(auto &&s, auto &&id) : base{s, id} {
        executable = binary_dir / "bin" / (string)name;
        ::sw::visit(bs.os, [&](auto &&os) {
            if constexpr (requires {os.executable_extension;}) {
                executable += os.executable_extension;
            }
        });
        ::sw::visit(
            bs.linker,
            [&](linker::msvc &c) {
                auto &t = s.load_target(c.package, bs);
                add(linker::msvc::rule_type{t});
            },
            [&](linker::gcc &c) {
                auto &t = s.load_target(c.package, bs);
                add(linker::gcc::rule_type{t});
            },
            [&](linker::gpp &c) {
                auto &t = s.load_target(c.package, bs);
                add(linker::gpp::rule_type{t});
            },
            [](auto &) {
                SW_UNIMPLEMENTED;
            });
    }

    void run(auto && ... args) {
        // make rule?
    }
};
using executable = executable_target;

using target = target_type::variant_type;

} // namespace sw
