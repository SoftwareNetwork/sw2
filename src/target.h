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
struct system_link_library {
    path p;
    operator const auto &() const { return p; }
};
auto operator""_slib(const char *s, size_t len) {
    return system_link_library{std::string{s, len}};
}

struct compile_options_t {
    std::vector<definition> definitions;
    std::vector<path> include_directories;
};
struct link_options_t {
    std::vector<path> link_directories;
    std::vector<path> link_libraries;
    std::vector<system_link_library> system_link_libraries;
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

    void add(const file_regex &r) {
        r(source_dir, [&](auto &&iter) {
            for (auto &&e : iter) {
                if (fs::is_regular_file(e)) {
                    add(e);
                }
            }
        });
    }
    void add(const path &p) {
        files.insert(p.is_absolute() ? p : source_dir / p);
    }
    void remove(const path &p) {
        files.erase(p.is_absolute() ? p : source_dir / p);
    }

    // this auto &&self,
    auto operator+=(auto &&v) {
        add(v);
        return appender{[&](auto &&v) { add(v); }};
    }
    auto operator-=(auto &&v) {
        remove(v);
        return appender{[&](auto &&v) { remove(v); }};
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
