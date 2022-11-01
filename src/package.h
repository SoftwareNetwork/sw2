// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2022 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include "helpers.h"

namespace sw {

struct istring : string {
    using string::string;
    using string::operator=;

    istring(const string &s) : string{s} {}

    std::strong_ordering operator<=>(const istring &rhs) const {
        auto r = stricmp(data(), rhs.data());
        if (r == 0) {
            return std::strong_ordering::equal;
        }
        return r < 0 ? std::strong_ordering::less : std::strong_ordering::greater;
    }

    auto hash() const {
        string v = *this;
        std::transform(v.begin(), v.end(), v.begin(), tolower);
        return std::hash<string>()(v);
    }
};

struct package_path {
    std::vector<istring> elements;

    package_path() {
    }
    package_path(const string &s) : elements{s} {
    }

    operator string() const {
        if (!elements.empty()) {
            return elements[0];
        }
        return "";
    }

    auto hash() const {
        size_t h = 0;
        for (auto &&e : elements) {
            h ^= e.hash();
        }
        return h;
    }

    auto operator<=>(const package_path &) const = default;
};

auto split_string(const string &s, string_view split) {
    std::vector<string_view> r;
    size_t p = 0;
    size_t pold = 0;
    while ((p = s.find(split, pold)) != -1) {
        r.emplace_back(s.data() + pold, s.data() + p);
        pold = p + split.size();
    }
    r.emplace_back(s.data() + pold, s.data() + s.size());
    return r;
}

struct package_version {
    struct number_version {
        struct numbers {
            using number_type = int;

            std::vector<number_type> value;

            numbers() = default;
            numbers(const std::initializer_list<number_type> &s) : value{s} {}
            numbers(const string &s) {
                std::ranges::copy(split_string(s, "."sv) | std::views::transform([&](auto &&v) {
                    number_type i;
                    auto [_,ec] = std::from_chars(v.data(), v.data() + v.size(), i);
                    if (ec != std::errc{}) {
                        throw std::runtime_error{"bad version"};
                    }
                    return i;
                }), std::back_inserter(value));
            }

            auto operator<=>(const numbers &rhs) const {
                return std::tie(value) <=> std::tie(rhs.value);
            }

            auto hash() const {
                size_t h = 0;
                for (auto &&e : value) {
                    h ^= std::hash<number_type>()(e);
                }
                return h;
            }
        };
        numbers elements;
        string extra;

        bool is_pre_release() const { return !extra.empty(); }
        bool is_release() const { return !is_pre_release(); }

        bool operator<(const number_version &rhs) const {
            if (is_release() && rhs.is_release()) {
                return elements < rhs.elements;
            } else if (is_release()) {
                return false;
            } else if (rhs.is_release()) {
                return true;
            } else {
                return std::tie(elements, extra) < std::tie(rhs.elements, rhs.extra);
            }
        }

        auto hash() const {
            return elements.hash() ^ std::hash<string>()(extra);
        }
    };
    using version_type = std::variant<number_version, string>;
    version_type version;

    package_version() : version{number_version{{0,0,1}}} {
    }
    package_version(const std::initializer_list<number_version::numbers::number_type> &s) : version{number_version{s}} {
    }
    package_version(const string &s) {
        if (s.empty()) {
            throw std::runtime_error{"empty version"};
        }
        auto is_alpha = [](char c) {
            return c >= 'a' && c <= 'z' || c >= 'A' && c <= 'Z' || c == '_';
        };
        auto alnum = std::ranges::all_of(s, [&](auto &&c) {
            return c >= '0' && c <= '9' || is_alpha(c);
        });
        if (alnum && is_alpha(s[0] == '_')) {
            version = s;
        } else {
            version = number_version{s};
        }
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

    // this is release order
    // sometimes we want semver order - todo: implement
    bool operator<(const package_version &rhs) const {
        if (is_version() && rhs.is_version()) {
            return std::tie(version) < std::tie(rhs.version);
        } else if (is_version()) {
            return false;
        } else if (rhs.is_version()) {
            return true;
        } else {
            return std::tie(version) < std::tie(rhs.version);
        }
    }

    auto hash() const {
        if (is_version()) {
            return std::get<number_version>(version).hash();
        }
        return std::hash<string>()(std::get<string>(version));
    }
};

struct version_range {
    using version = package_version::number_version::numbers;
    struct pair : std::pair<version, version> {
        bool contains(const package_version::number_version &v) {
            return first <= v.elements && v.elements <= second;
        }
    };

    // change to set
    std::vector<pair> pairs;

    bool contains(const package_version::number_version &v) {
        return std::ranges::any_of(pairs, [&](auto &&p) { return p.contains(v); });
    }
};

struct package_version_range {
    using range_type = std::variant<version_range, string>;
    range_type range;

    bool is_branch() const {
        return std::holds_alternative<string>(range);
    }
};

struct package_name {
    package_path path;
    package_version version;

    package_name() = default;
    package_name(const char *s) : package_name{string{s}} {
    }
    package_name(const string &s) : path{s} {
    }
    package_name(const string &p, const string &v) : path{p}, version{v} {
    }
    void operator=(const string &s) {
        path = s;
    }

    operator string() const { return path; }

    auto hash() const {
        return path.hash() ^ version.hash();
    }

    bool operator<(const package_name &rhs) const {
        return std::tie(path, version) < std::tie(rhs.path, rhs.version);
    }
};

struct unresolved_package_name {
    package_name name;
    package_version_range range;

    bool contains(const package_version &v) {
        if (v.is_branch() && range.is_branch()) {
            return std::get<string>(v.version) == std::get<string>(range.range);
        }
        if (v.is_branch() || range.is_branch()) {
            return false;
        }
        return std::get<version_range>(range.range).contains(std::get<package_version::number_version>(v.version));
    }
};

}
