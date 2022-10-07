// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2022 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include "helpers.h"
#include "package.h"
#include "suffix.h"

namespace sw {

struct definition {
    string key;
    std::variant<string, bool> value; // value/undef

    operator string() const {
        return visit(value, overload{
            [&](const string &v){ return "-D" + key + "=" + v; },
            [&](bool){ return "-U" + key; }
        });
    }
};
struct compile_options_t {
    std::vector<definition> definitions;
    std::vector<path> include_directories;
};
struct link_options_t {
    std::vector<path> link_directories;
    std::vector<path> link_libraries;
};

// binary_target_package?
struct cl_binary_target : compile_options_t, link_options_t {
    sw::package_id package;
    path exe;
};
struct binary_library_target : compile_options_t, link_options_t {
};

struct files_target {
    using files_t = std::set<path>; // unordered?
    package_id name;
    path source_dir;
    files_t files;

    void op(file_regex &r, auto &&f) {
        r(source_dir, [&](auto &&iter) {
            for (auto &&e : iter) {
                if (fs::is_regular_file(e)) {
                    f(files, e);
                }
            }
        });
    }
    void op(const path &p, auto &&f) {
        f(files, p);
    }
    auto operator+=(auto &&iter) {
        op(iter, [&](auto &&arr, auto &&f) {
            arr.insert(source_dir / f);
        });
        return appender{[&](auto &&v) {
            operator+=(v);
        }};
    }
    auto operator-=(auto &&iter) {
        op(iter, [&](auto &&arr, auto &&f) {
            arr.erase(source_dir / f);
        });
        return appender{[&](auto &&v) {
            operator-=(v);
        }};
    }
    auto range() const {
        return files | std::views::transform([&](auto &&p) {
                   return source_dir / p;
               });
    }
    auto begin() const {
        return iter_with_range{range()};
    }
    auto end() const {
        return files.end();
    }
};

}
