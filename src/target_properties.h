// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2022 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include "helpers/common.h"

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
inline definition operator""_def(const char *in, size_t len) {
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
inline auto operator""_idir(const char *s, size_t len) {
    return include_directory{std::string{s,len}};
}

struct compile_option {
    string value;
    operator auto() const { return value; }
};
inline auto operator""_copt(const char *s, size_t len) {
    return compile_option{std::string{s, len}};
}

struct link_option {
    string value;
    operator auto() const { return value; }
};
inline auto operator""_lopt(const char *s, size_t len) {
    return link_option{std::string{s, len}};
}

struct link_library {
    path p;
    link_library(const char *p) : p{p} {}
    link_library(const path &p) : p{p} {}
    operator const auto &() const {
        return p;
    }
};

struct system_link_library {
    path p;
    operator const auto &() const {
        return p;
    }
};
inline auto operator""_slib(const char *s, size_t len) {
    return system_link_library{std::string{s, len}};
}

struct force_include {
    path p;
    operator const auto &() const {
        return p;
    }
};
inline auto operator""_fi(const char *s, size_t len) {
    return force_include{std::string{s, len}};
}

struct precompiled_header_raw {
    path header;
    // msvc
    bool create{true};
    bool use{true};
    path cpp;
    path pch;
    path pdb;
    path obj;

    operator const auto &() const {
        return header;
    }
};
struct precompiled_header {
    path p;
    operator const auto &() const {
        return p;
    }
};
inline auto operator""_pch(const char *s, size_t len) {
    return precompiled_header{std::string{s, len}};
}

struct compile_options_t {
    std::vector<definition> definitions;
    std::vector<include_directory> include_directories;
    std::vector<compile_option> compile_options;

    void merge(auto &&from) {
        append_vector(definitions, from.definitions);
        append_vector(include_directories, from.include_directories);
        append_vector(compile_options, from.compile_options);
    }
};
struct link_options_t {
    std::vector<path> link_directories;
    std::vector<link_library> link_libraries;
    std::vector<system_link_library> system_link_libraries;
    std::vector<link_option> link_options;

    void merge(auto &&from) {
        append_vector(link_directories, from.link_directories);
        append_vector(link_libraries, from.link_libraries);
        append_vector(system_link_libraries, from.system_link_libraries);
        append_vector(link_options, from.link_options);
    }
};

struct file_regex {
    std::string str;
    bool recursive;

    void operator()(auto &&rootdir, auto &&f) const {
        auto &&[root,regex] = extract_dir_regex(str);
        if (root.is_absolute()) {
            throw;
        }
        root = rootdir / root;
        auto range = [&]<typename Iter>() {
            return
                // add caching? yes
                // add win fast path? yes
                std::ranges::owning_view{Iter{root}}
                | std::views::filter([dir = root.string(), r = std::regex{regex}](auto &&el){
                    auto s = el.path().string();
                    if (s.starts_with(dir)) {
                        // eat one more slash
                        s = s.substr(dir.size() + 1);
                    } else {
                        throw;
                    }
                    return std::regex_match(s, r);
                })
                ;
        };
        if (recursive) {
            f(range.template operator()<fs::recursive_directory_iterator>());
        } else {
            f(range.template operator()<fs::directory_iterator>());
        }
    }
    static auto extract_dir_regex(std::string_view fn) {
        path dir;
        std::string regex_string;
        size_t p = 0;
        do
        {
            auto p0 = p;
            p = fn.find_first_of("/*?+[.\\", p);
            if (p == -1 || fn[p] != '/') {
                regex_string = fn.substr(p0);
                return std::tuple{dir, regex_string};
            }

            // scan first part for '\.' pattern that is an exact match for point
            // other patterns? \[ \( \{

            auto sv = fn.substr(p0, p++ - p0);
            string s{sv.begin(), sv.end()};
            // protection of valid path symbols into regex
            s = replace(s, "\\.", ".");
            s = replace(s, "\\[", "[");
            s = replace(s, "\\]", "]");
            s = replace(s, "\\(", "(");
            s = replace(s, "\\)", ")");
            s = replace(s, "\\{", "{");
            s = replace(s, "\\}", "}");

            if (s.find_first_of("*?+.[](){}") != -1) {
                regex_string = fn.substr(p0);
                return std::tuple{dir, regex_string};
            }

            // windows fix
            if (!s.empty() && s.back() == ':') {
                s += '/';
            }

            dir /= s;
        } while (1);
    }
};

inline auto operator""_dir(const char *s, size_t len) {
    return file_regex(std::string{s,len} + "/.*",false);
}
inline auto operator""_rdir(const char *s, size_t len) {
    return file_regex(std::string{s,len} + "/.*", true);
}
inline auto operator""_r(const char *s, size_t len) {
    return file_regex(std::string{s,len},false);
}
inline auto operator""_rr(const char *s, size_t len) {
    return file_regex(std::string{s,len}, true);
}

} // namespace sw
