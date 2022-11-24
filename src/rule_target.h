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

struct native_target : rule_target {
    using base = rule_target;

    using base::operator+=;
    using base::add;
    using base::remove;

    compile_options_t compile_options;
    link_options_t link_options;

    native_target(auto &&s, auto &&id) : base{s, id} {
        *this += native_sources_rule{};

        ::sw::visit(bs.c_compiler, [&](c_compiler::msvc &c) {
            get_msvc_detector().add(s);
            s.load_target(c.package, s.host_settings());
            //add_rule();
            ;
        },
        [](auto &) {
            SW_UNIMPLEMENTED;
        });
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

using target_type = types<files_target, rule_target, native_target, executable_target, binary_target, binary_library_target>;
using target = target_type::variant_type;
using target_ptr = target_type::variant_of_uptr_type;

} // namespace sw
