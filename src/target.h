#pragma once

#include "helpers.h"
#include "package.h"

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
struct link_options_t {};

// binary_target_package?
struct cl_binary_target : compile_options_t, link_options_t {
    sw::package_id package;
    path exe;
};
struct binary_library_target : compile_options_t, link_options_t {
};
