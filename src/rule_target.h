// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2022 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include "rule.h"

namespace sw {

struct rule_target : files_target {
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
    using files_target::operator+=;
    template <typename T>
    auto operator+=(T &&r) requires requires {requires rule_types::contains<T>();} {
        add_rule(r);
        return appender{[&](auto &&v){operator+=(v);}};
    }

    void operator()() {
        command_executor ce;
        ce.run(*this);
    }
};

}
