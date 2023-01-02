// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2022 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include "helpers.h"

namespace sw {

struct definition {
    string key;
    std::optional<string> value; // value/undef

    operator string() const {
        // add undefs? "-U" + key

        // we have following forms with different meaning
        // -DKEY        means KEY=1
        // -DKEY=       means KEY= (exact nothing, only a fact)
        // -DKEY=VALUE  means KEY=VALUE - usual case

        string s;
        s += "-D" + key;
        if (value) {
            s += "=" + *value;
            // handle spaces
            /*auto v2 = v.toString();
        auto has_spaces = true;
        // new win sdk contains rc.exe that can work without quotes around def values
        // we should check rc version here, if it > winsdk 10.19041, then run the following line
        has_spaces = std::find(v2.begin(), v2.end(), ' ') != v2.end();
        // some targets gives def values with spaces
        // like pcre 'SW_PCRE_EXP_VAR=extern __declspec(dllimport)'
        // in this case we protect the value with quotes
        if (has_spaces && v2[0] != '\"')
            s += "\"";
        s += v2;
        if (has_spaces && v2[0] != '\"')
            s += "\"";*/
        }
        return s;
    }
};
definition operator""_def(const char *in, size_t len) {
    string d = in;
    auto p = d.find('=');
    if (p == d.npos)
        return {d, {}}; // = 1
    auto f = d.substr(0, p);
    auto s = d.substr(p + 1);
    if (s.empty()) {
        return {f, string{}};
    } else {
        return {f, s};
    }
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
