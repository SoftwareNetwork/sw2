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

    using files_target::operator+=;
    using files_target::add;
    using files_target::remove;

    void add(this auto &&self, const rule &r) {
        std::visit([&](auto &&v){v(self);}, r);
        for (auto &&c : self.commands) {
            visit(c, [&](auto &&c) {
                for (auto &&o : c.outputs) {
                    self.processed_files[o];
                }
            });
        }
    }

    void operator()(this auto &&self) {
        command_executor ce;
        ce.run(self);
    }

    //auto visit()
};

struct native_target : rule_target {
    compile_options_t compile_options;
    link_options_t link_options;

    using rule_target::operator+=;
    using rule_target::add;
    using rule_target::remove;

    native_target() {
        *this += native_sources_rule{};
    }

    void add(const system_link_library &l) {
        link_options.system_link_libraries.push_back(l);
    }
};

using target = variant<files_target, rule_target, native_target>;

}
