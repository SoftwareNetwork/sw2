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

struct package_version {
    struct number_version {
        using numbers = std::vector<int>;
        numbers elements;
        string extra;
    };
    std::variant<number_version, string> version;

    package_version() : version{number_version{{0,0,1}}} {
    }
    package_version(const string &s) {
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
