// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2022 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include "helpers.h"

namespace sw {

struct istring : string {
    using string::string;
    using string::operator=;

     //std::strong_ordering operator<=>(const istring &rhs) const { return stricmp(data(), rhs.data()); }
};

struct package_name {
    //std::vector<istring> elements;
    std::vector<string> elements;

    package_name() {
    }
    package_name(const string &s) : elements{s} {
    }

    operator string() const {
        if (!elements.empty()) {
            return elements[0];
        }
        return "";
    }
};

auto split_string(const string &s, string_view split) {
    std::vector<string_view> r;
    size_t p = 0;
    while ((p = s.find(split)) != -1) {
    }
    return r;
}

struct package_version {
    struct number_version {
        struct numbers {
            std::vector<int> value;

            numbers() = default;
            numbers(const std::initializer_list<int> &s) : value{s} {}
            numbers(const string &s) {
                for (auto &&e : split_string(s, "."sv)) {

                }
            }
        };
        numbers elements;
        string extra;

        bool is_pre_release() const { return !extra.empty(); }
        bool is_release() const { return !is_pre_release(); }
    };
    using version_type = std::variant<number_version, string>;
    version_type version;

    package_version() : version{number_version{{0,0,1}}} {
    }
    package_version(const string &s) {
    }
    package_version(const version_type &s) : version{s} {
    }

    bool is_pre_release() const {
        return std::holds_alternative<number_version>(version) && std::get<number_version>(version).is_pre_release();
    }
    bool is_release() const {
        return std::holds_alternative<number_version>(version) && std::get<number_version>(version).is_release();
    }
    bool is_version() const {
        return std::holds_alternative<number_version>(version);
    }
    bool is_branch() const {
        return std::holds_alternative<string>(version);
    }
};

struct package_id {
    package_name name;
    package_version version;

    package_id() = default;
    package_id(const string &s) : name{s} {
    }
    package_id(const string &p, const string &v) : name{p}, version{v} {
    }
    void operator=(const string &s) {
        name = s;
    }

    operator string() const { return name; }
};

}
