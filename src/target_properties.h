// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2022 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include "helpers.h"

namespace sw {

struct definition {
    string key;
    std::variant<string, bool> value; // value/undef

    operator string() const {
        return visit(value, overload{[&](const string &v) {
                                         return "-D" + key + "=" + v;
                                     },
                                     [&](bool) {
                                         return "-U" + key;
                                     }});
    }
};
auto operator""_def(const char *s, size_t len) {
    return definition{std::string{s, len}};
}

struct include_directory {
    path dir;
    include_directory() = default;
    include_directory(const path &p) : dir{p} {}
    operator auto() const { return dir; }
};
auto operator""_idir(const char *s, size_t len) {
    return include_directory{std::string{s,len}};
}

struct compile_option {
    string value;
    operator auto() const { return value; }
};
auto operator""_copt(const char *s, size_t len) {
    return compile_option{std::string{s, len}};
}

struct system_link_library {
    path p;
    operator const auto &() const {
        return p;
    }
};
auto operator""_slib(const char *s, size_t len) {
    return system_link_library{std::string{s, len}};
}

struct compile_options_t {
    std::vector<definition> definitions;
    std::vector<include_directory> include_directories;
    std::vector<compile_option> compile_options;
};
struct link_options_t {
    std::vector<path> link_directories;
    std::vector<path> link_libraries;
    std::vector<system_link_library> system_link_libraries;
};

} // namespace sw
