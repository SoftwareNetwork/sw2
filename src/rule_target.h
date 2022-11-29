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

    using base::operator+=;
    using base::add;
    using base::remove;

    build_settings bs; // ref? but how to deal in visit const/non const?
    path binary_dir;
    command_storage cs;
    std::vector<rule> rules;
    std::map<path, rule_flag> processed_files;
    std::vector<command> commands;

    rule_target(auto &&solution, auto &&id)
        : files_target{id}
        , bs{*solution.bs}
        , binary_dir{make_binary_dir(solution.binary_dir)}
        , cs{binary_dir}
   {
        source_dir = solution.source_dir;
    }
    auto make_binary_dir(const path &parent) {
        auto config = bs.hash();
        return parent / "t" / std::to_string(config) / std::to_string(name.hash());
    }

    auto &build_settings() {
        return bs;
    }
    auto &build_settings() const {
        return bs;
    }

    template <typename T, typename ... Types>
    bool is() { return bs.is<T, Types...>(); }
    void visit(auto && ... f) {
        bs.visit(FWD(f)...);
    }

    void add(const rule &r) {
        rules.push_back(r);
    }
    void init_rules(this auto &&self) {
        for (auto &&r : self.rules) {
            std::visit([&](auto &&v){
                if constexpr (requires {v(self);}) {
                    v(self);
                }
            }, r);
            for (auto &&c : self.commands) {
                ::visit(c, [&](auto &&c) {
                    for (auto &&o : c.outputs) {
                        self.processed_files[o];
                    }
                });
            }
        }
        for (auto &&c : self.commands) {
            ::visit(c, [&](auto &&c2) {
                c2.cs = &self.cs;
            });
        }
    }

    void prepare(this auto &&self) {
        self.init_rules();
    }
    void build(this auto &&self) {
        self.prepare();

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

    using base::operator+=;
    using base::add;
    using base::remove;

    //
    bool mt{false};

    native_target(auto &&s, auto &&id) : base{s, id} {
        *this += native_sources_rule{};
        init_compilers(s);

        bs.os.visit_any([&](os::windows) {
            *this += "-DSW_EXPORT=__declspec(dllexport)"_def;
            *this += "-DSW_IMPORT=__declspec(dllimport)"_def;
        });
    }

    void init_compilers(auto &&s) {
        auto load = [&] {
            std::once_flag once;
            auto load = [&] {
                detect_winsdk(s);
                get_msvc_detector().add(s);
            };
            visit_any(bs.c_compiler, [&](c_compiler::msvc &c) {
                std::call_once(once, load);
            });
            visit_any(bs.cpp_compiler, [&](cpp_compiler::msvc &c) {
                std::call_once(once, load);
            });
            visit_any(bs.linker, [&](linker::msvc &c) {
                std::call_once(once, load);
            });
        };
        std::call_once(s.system_targets_detected, load);

        // order
        add(s.load_target(bs.cpp_stdlib, bs));
        add(s.load_target(bs.c_stdlib, bs));
        add(s.load_target(bs.kernel_lib, bs));

        ::sw::visit(
            bs.c_compiler,
            [&](c_compiler::msvc &c) {
                auto &t = s.load_target(c.package, bs);
                add(c_compiler::msvc::rule_type{t});
            },
            [](auto &) {
                SW_UNIMPLEMENTED;
            });
        ::sw::visit(
            bs.cpp_compiler,
            [&](cpp_compiler::msvc &c) {
                auto &t = s.load_target(c.package, bs);
                add(cpp_compiler::msvc::rule_type{t});
            },
            [](auto &) {
                SW_UNIMPLEMENTED;
            });
        ::sw::visit(
            bs.linker,
            [&](linker::msvc &c) {
                auto &t = s.load_target(c.package, bs);
                add(linker::msvc::rule_type{t});
            },
            [](auto &) {
                SW_UNIMPLEMENTED;
            });
    }

    void add(target_uptr &ptr) {
        dependencies.push_back({&ptr});
    }
    void add(const system_link_library &l) {
        link_options.system_link_libraries.push_back(l);
    }
    void add(const definition &d) {
        compile_options.definitions.push_back(d);
    }

    //void build() { operator()(); }
    //void run(){}
};

struct native_library_target : native_target {
    using base = native_target;
    path library;
    path implib;
    std::optional<bool> shared_;

    native_library_target(auto &&s, auto &&id, const std::optional<bool> &shared = {}) : base{s, id}, shared_{shared} {
        if (!shared_) {
            shared_ = bs.library_type.visit(
                [](library_type::static_ &){
                    return false;
                },
                [](library_type::shared &) {
                    return true;
                }, [](auto &) {
                    return true; // for now shared if the default
                }
            );
        }
        if (is_shared()) {
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
        } else {
            library = binary_dir / "lib" / (string)name;
            ::sw::visit(bs.os, [&](auto &&os) {
                if constexpr (requires { os.static_library_extension; }) {
                    library += os.static_library_extension;
                }
            });
        }
    }
    bool is_shared() const {
        return *shared_;
    }
};
struct native_shared_library_target : native_library_target {
    native_shared_library_target(auto &&s, auto &&id) : base{s, id, true} {
        bs.os.visit_any([&](os::windows) {
            *this += "_WINDLL"_def;
        });
    }
};
struct native_static_library_target : native_library_target {
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
    }

    void run(auto && ... args) {
        // make rule?
    }
};

using target = target_type::variant_type;

} // namespace sw
