// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2022 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include "rule.h"

namespace sw {

// basic target: throw files, rules etc.
struct rule_target : files_target {
    path binary_dir;
    std::vector<rule> rules;
    std::map<path, rule_flag> processed_files;
    std::vector<command> commands;

    compile_options_t compile_options;
    link_options_t link_options;

    /*void add_rule(auto &&r) {
        r(*this);
    }*/
    void add_rule(const rule &r) {
        std::visit([&](auto &&v){v(*this);}, r);
        for (auto &&c : commands) {
            visit(c, [&](auto &&c) {
                for (auto &&o : c.outputs) {
                    processed_files[o];
                }
            });
        }
    }
    template <typename T>
    auto operator+=(T &&r) requires requires {requires rule_types::contains<std::decay_t<T>>();} {
        add_rule(r);
        return appender{[&](auto &&v){operator+=(v);}};
    }
    void add(const system_link_library &l) {
        link_options.system_link_libraries.push_back(l);
    }
    using files_target::operator+=;
    using files_target::add;
    using files_target::remove;

    auto operator+=(auto &&v) {
        add(v);
        return appender{[&](auto &&v) { add(v); }};
    }
    auto operator-=(auto &&v) {
        remove(v);
        return appender{[&](auto &&v) { remove(v); }};
    }

    void operator()() {
        command_executor ce;
        ce.run(*this);
    }

    //auto visit()
};

struct native_target {
};

using target = variant<files_target, rule_target>;

}
