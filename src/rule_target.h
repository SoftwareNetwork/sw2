// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2022 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include "rule.h"
#include "os.h"

namespace sw {

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
    std::vector<target_ptr *> dependencies;
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
    target_data &interface_{get(groups::project | groups::others)};

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

    native_target(auto &&s, auto &&id) : base{s, id} {
        *this += native_sources_rule{};

        std::once_flag win_sdk_um, win_ucrt;
        auto load_win_sdk_um = [&] {
            detect_winsdk(s);
            auto &t = s.load_target(unresolved_package_name{"com.Microsoft.Windows.SDK.um", "*"}, bs);
            add(t);
        };
        auto load_win_ucrt = [&] {
            detect_winsdk(s);
            auto &t = s.load_target(unresolved_package_name{"com.Microsoft.Windows.SDK.ucrt", "*"}, bs);
            add(t);
        };

        ::sw::visit(bs.c_compiler,
            [&](c_compiler::msvc &c) {
            get_msvc_detector().add(s);
            auto &t = s.load_target(c.package, s.host_settings());
            add(c_compiler::msvc::rule_type{*std::get<std::unique_ptr<binary_target_msvc>>(t)});
            std::call_once(win_sdk_um, load_win_sdk_um);
            std::call_once(win_ucrt, load_win_ucrt);
            },
        [](auto &) {
            SW_UNIMPLEMENTED;
            });
        ::sw::visit(
            bs.cpp_compiler,
            [&](cpp_compiler::msvc &c) {
                get_msvc_detector().add(s);
                auto &t = s.load_target(c.package, s.host_settings());
                add(cpp_compiler::msvc::rule_type{*std::get<std::unique_ptr<binary_target_msvc>>(t)});
                std::call_once(win_sdk_um, load_win_sdk_um);
                std::call_once(win_ucrt, load_win_ucrt);
            },
            [](auto &) {
                SW_UNIMPLEMENTED;
            });
    }

    void add(target_ptr &t) {
        dependencies.push_back(&t);
    }
    void add(const system_link_library &l) {
        link_options.system_link_libraries.push_back(l);
    }

    //void build() { operator()(); }
    //void run(){}

    /*static void detect_system_targets(auto &&s) {
        if (!s.system_targets_detected) {
            detect_msvc(s);
            detect_winsdk(s);
            s.system_targets_detected = true;
        }
    }*/
};

struct executable_target : native_target {
    using base = native_target;

    executable_target(auto &&s, auto &&id) : base{s, id} {
        executable = binary_dir / "bin" / (string)name += s.os.executable_extension;
    }

    path executable;

    void run(auto && ... args) {
        // make rule?
    }
};

using target_type = types<files_target, rule_target, native_target, executable_target, binary_target, binary_library_target, binary_target_msvc>;
using target = target_type::variant_type;
using target_ptr = target_type::variant_of_uptr_type;

} // namespace sw
