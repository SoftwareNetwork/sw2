// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2022 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include "helpers/common.h"

namespace sw {

#if defined(_WIN32) && 0
struct istring : string {
    using string::string;
    using string::operator=;

    istring(const string &s) : string{s} {}

    std::strong_ordering operator<=>(const istring &rhs) const {
#ifdef _WIN32
        auto r = stricmp(data(), rhs.data());
#else
        auto r = strcasecmp(data(), rhs.data());
#endif
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
#else
using istring = string;
#endif

struct package_path {
    std::vector<istring> elements;

    package_path() {
    }
    package_path(const char *s) : elements{s} {
    }
    package_path(const string &s) : elements{s} {
    }
    //package_path(const package_path &) = default;
    //package_path &operator=(const package_path &) = default;

    operator string() const {
        if (!elements.empty()) {
            return elements[0];
        }
        return "";
    }

    auto hash() const {
        size_t h = 0;
        for (auto &&e : elements) {
            h = hash_combine(h, e);
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

            auto operator<=>(const numbers &rhs) const = default;

            auto hash() const {
                size_t h = 0;
                for (auto &&e : value) {
                    h = hash_combine(h, e);
                }
                return h;
            }
            auto size() const {
                return value.size();
            }
        };
        numbers elements;
        string extra;

        bool is_pre_release() const { return !extra.empty(); }
        bool is_release() const { return !is_pre_release(); }

        auto operator<=>(const number_version &) const = default;

        auto hash() const {
            return elements.hash() ^ std::hash<string>()(extra);
        }

        operator string() const {
            string s;
            for (auto &&i : elements.value) {
                s += std::to_string(i) + ".";
            }
            if (!s.empty()) {
                s.resize(s.size() - 1);
            }
            if (!extra.empty()) {
                s += "-" + extra;
            }
            return s;
        }

        auto &operator[](int i) const {
            return elements.value.at(i);
        }
        auto size() const {
            return elements.size();
        }
    };
    using version_type = std::variant<string, number_version>;
    version_type version;

    package_version() : version{number_version{{0,0,1}}} {
    }
    package_version(const std::initializer_list<number_version::numbers::number_type> &s) : version{number_version{s}} {
    }
    package_version(const char *s) : package_version{string{s}} {
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
    package_version(const package_version::number_version &s) : version{s} {
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

    operator string() const {
        if (is_branch()) {
            return std::get<string>(version);
        }
        return std::get<number_version>(version);
    }
    std::string to_string_initializer_list() const {
        auto &v = std::get<number_version>(version);
        std::string s;
        for (auto &&e : v.elements.value) {
            s += std::format("{},", e);
        }
        if (!s.empty()) {
            s.pop_back();
        }
        return std::format("{{{}}}", s);
    }

    // this is release order
    // sometimes we want semver order - todo: implement
    auto operator<=>(const package_version &) const = default;

    auto hash() const {
        if (is_version()) {
            return std::get<number_version>(version).hash();
        }
        return std::hash<string>()(std::get<string>(version));
    }

    auto &operator[](int i) const {
        return std::get<number_version>(version)[i];
    }
    auto size() const {
        return std::get<number_version>(version).size();
    }
};

package_version operator""_ver(const char *s, size_t len) {
    return {s};
}

struct version_range {
    using version = package_version::number_version::numbers;
    struct pair : std::pair<package_version::number_version, package_version::number_version> {
        using base = std::pair<package_version::number_version, package_version::number_version>;
        using base::base;
        bool contains(const package_version::number_version &v) const {
            // <= v < ? but harder to implement?
            return first <= v && v <= second;
        }
        auto operator<(const pair &rhs) const {
            return first < rhs.first;
        }

        auto hash() const {
            size_t h = 0;
            h = hash_combine(h, first.hash());
            h = hash_combine(h, second.hash());
            return h;
        }
    };

    // change to set
    std::vector<pair> pairs;

    bool contains(const package_version::number_version &v) const {
        return std::ranges::any_of(pairs, [&](auto &&p) { return p.contains(v); });
    }
    operator string() const {
        string s;
        for (auto &&[f,t] : pairs) {
            s += "[" + string{f} + ", " + string{t} + "] |";
        }
        if (!s.empty()) {
            s.resize(s.size() - 2);
        }
        return s;
    }

    auto hash() const {
        size_t h = 0;
        for (auto &&p : pairs) {
            h = hash_combine(h, p.hash());
        }
        return h;
    }

    auto operator<(const version_range &rhs) const {
        return pairs < rhs.pairs;
    }
};

struct package_version_range {
    using range_type = std::variant<version_range, string>;
    range_type range;

    package_version_range() : package_version_range{"*"s} {
    }
    package_version_range(const char *s) : package_version_range{string{s}} {
    }
    package_version_range(std::string_view s) : package_version_range{string{s.begin(), s.end()}} {
    }
    package_version_range(const package_version &v) {
        if (v.is_branch()) {
            range = v;
        } else {
            range = version_range{.pairs = {{
                std::get<package_version::number_version>(v.version),
                std::get<package_version::number_version>(v.version)
                }}};
        }
    }
    package_version_range(const std::string &s) {
        if (s == "*") {
            package_version::number_version from{{0}};
            package_version::number_version to{{999999999}};
            version_range::pair p{from,to};
            range = version_range{.pairs = {p}};
        } else {
            range = s;
        }
    }

    bool is_branch() const {
        return std::holds_alternative<string>(range);
    }
    bool contains(const package_version &v) const {
        auto b = is_branch();
        auto bv = v.is_branch();
        if (b && bv) {
            return std::get<string>(range) == std::get<string>(v.version);
        }
        if (b ^ bv) {
            return false;
        }
        return std::get<version_range>(range).contains(std::get<package_version::number_version>(v.version));
    }
    operator string() const {
        if (is_branch()) {
            return std::get<string>(range);
        }
        return std::get<version_range>(range);
    }

    auto hash() const {
        size_t h = 0;
        if (is_branch()) {
            h = hash_combine(h, std::get<string>(range));
        } else {
            h = hash_combine(h, std::get<version_range>(range).hash());
        }
        return h;
    }

    auto operator<(const package_version_range &rhs) const {
        return range < rhs.range;
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
    package_name(const string &p, const package_version &v) : path{p}, version{v} {
    }
    void operator=(const string &s) {
        path = s;
    }

    operator string() const { return (string)path + "-" + (string)version; }

    auto hash() const {
        return path.hash() ^ version.hash();
    }
    bool operator<(const package_name &rhs) const {
        return std::tie(path, version) < std::tie(rhs.path, rhs.version);
    }
    auto to_string() const { return (string)*this; }
    auto toString() const { return to_string(); } // v1 compat
};

struct unresolved_package_name {
    package_path path;
    package_version_range range;

    unresolved_package_name() = default;
    unresolved_package_name(const char *p) : path{p} {}
    unresolved_package_name(const std::string &p) : path{p} {}
    unresolved_package_name(auto &&p, auto &&r) : path{p}, range{r} {}
    unresolved_package_name(const package_name &p) : path{p.path}, range{p.version} {}

    bool contains(const package_version &v) {
        if (v.is_branch() && range.is_branch()) {
            return std::get<string>(v.version) == std::get<string>(range.range);
        }
        if (v.is_branch() || range.is_branch()) {
            return false;
        }
        return std::get<version_range>(range.range).contains(std::get<package_version::number_version>(v.version));
    }
    operator string() const {
        string s = path;
        s += "-"s + string{range};
        return s;
    }

    auto hash() const {
        size_t h = path.hash();
        h = hash_combine(h, range.hash());
        return h;
    }

    auto operator<(const unresolved_package_name &rhs) const {
        return std::tie(path, range) < std::tie(rhs.path, rhs.range);
    }
};

template <typename T>
struct package_version_map {
    using mapped_type = T;
    using versions_type = std::map<package_version, mapped_type>;

    versions_type versions;

    auto &operator[](const package_version &version) {
        auto it = versions.find(version);
        if (it == versions.end()) {
            auto &&[it2, _] = versions.emplace(version, mapped_type{});
            it = it2;
        }
        return it->second;
    }
    auto find(const package_version_range &r) {
        return std::max_element(versions.begin(), versions.end(), [&](auto &&v1, auto &&v2) {
            return r.contains(v1.first) && r.contains(v2.first) && v1.first < v2.first;
        });
    }
    auto begin() {
        return versions.begin();
    }
    auto end() {
        return versions.end();
    }
    auto &container() {
        return versions;
    }
};
template <typename T>
struct package_map {
    using mapped_type = package_version_map<T>;
    using packages_type = std::map<package_path, mapped_type>;

    packages_type packages;

    auto &operator[](const package_name &name) {
        auto it = packages.find(name.path);
        if (it == packages.end()) {
            auto &&[it2, _] = packages.emplace(name.path, mapped_type{});
            it = it2;
        }
        return it->second[name.version];
    }
    auto &find(const unresolved_package_name &pkg) {
        auto it = packages.find(pkg.path);
        if (it == packages.end()) {
            throw std::runtime_error{"cannot load package: "s + string{pkg.path} + ": not found"};
        }
        auto it2 = it->second.find(pkg.range);
        if (it2 == it->second.end()) {
            throw std::runtime_error{"cannot load package: "s + string{pkg} + ": not found"};
        }
        return it2->second;
    }
    template <typename U>
    auto &find_first(const unresolved_package_name &pkg) {
        auto &&t = find(pkg);
        if (t.empty()) {
            // logic error?
            SW_UNIMPLEMENTED;
            // throw std::runtime_error{"cannot load package: "s + string{pkg} + ": not found"};
        }
        return *std::get<uptr<U>>(t.targets.begin()->second);
    }

    struct end_sentinel {};
    struct iterator {
        struct package_id_ref {
            const package_path *path;
            const package_version *version;
        };
        struct pair {
            package_id_ref first;
            T *second;
        };

        package_map *tm;
        packages_type::iterator p;
        mapped_type::versions_type::iterator v;
        pair data;

        iterator(package_map &tm) : tm{&tm} {
            init(*this->tm, p, v);
        }
        //iterator(const iterator &rhs) = default;
        //iterator &operator=(const iterator &rhs) = default;
        /*auto &operator=(const iterator &rhs) {
            p = rhs.p;
            v = rhs.v;
            return *this;
        }*/
        auto operator->() {
            data = pair{package_id_ref{&p->first, &v->first}, &v->second};
            return &data;
        }
        auto &operator*() {
            data = pair{package_id_ref{&p->first, &v->first}, &v->second};
            return data;
        }
        auto &operator++() {
            next(*tm, p, v);
            return *this;
        }
        bool operator==(const iterator &rhs) const {
            return std::tie(p, v) == std::tie(rhs.p, rhs.v);
        }
        bool operator==(end_sentinel) const {
            return p == tm->packages.end();
        }
        bool init(auto &&obj, auto &&it, auto &&...tail) {
            it = obj.container().begin();
            if constexpr (sizeof...(tail) > 0) {
                while (it != obj.container().end() && !init(it->second, FWD(tail)...)) {
                    ++it;
                }
            }
            return it != obj.container().end();
        }
        bool next(auto &&obj, auto &&it, auto &&...tail) {
            if constexpr (sizeof...(tail) > 0) {
                if (!next(it->second, FWD(tail)...)) {
                    ++it;
                    while (it != obj.container().end() && !init(it->second, FWD(tail)...)) {
                        ++it;
                    }
                    return it != obj.container().end();
                }
                return true;
            }
            return ++it != obj.container().end();
        }
    };
    auto begin() {
        return iterator{*this};
    }
    auto end() {
        return end_sentinel{};
    }
    auto &container() {
        return packages;
    }
};

}
