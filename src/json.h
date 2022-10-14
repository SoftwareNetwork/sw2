// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2022 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include "helpers.h"

namespace sw {

/*
some of replacements
    case 0:       stream << "\\0";  break;
    case 0x7:     stream << "\\a";  break;
    case 0x8:     stream << "\\b";  break;
    case 0x9:     stream << "\\t";  break;
    case 0xA:     stream << "\\n";  break;
    case 0xB:     stream << "\\v";  break;
    case 0xC:     stream << "\\f";  break;
    case 0xD:     stream << "\\r";  break;
    case 0x1B:    stream << "\\e";  break;
    case '"':     stream << "\\\""; break;
    case '\\':    stream << "\\\\"; break;
    case 0xA0:    stream << "\\_";  break;
    case 0x85:    stream << "\\N";  break;
    case 0x2028:  stream << "\\L";  break;
    case 0x2029:  stream << "\\P";  break;
*/

struct json {
    using array = vector<json>;
    using object = ::std::map<string_view, json>;
    // using simple_value = variant<string_view, int, double, bool, nullptr_t>;
    using simple_value = variant<string_view>;
    using value_type = variant<simple_value, array, object>;

    value_type value;

    auto &operator[](auto &&key) const {
        auto &p = std::get<object>(value);
        if (auto i = p.find(key); i != p.end()) {
            return i->second;
        }
        throw std::runtime_error{"not such key"};
    }
    operator string() const {
        string_view sv = *this;
        string s;
        s.reserve(sv.size());
        for (auto i = sv.begin(); i != sv.end(); ++i) {
            if (*i == '\\') {
                switch (*(i+1)) {
                case '\\': s += '\\'; ++i; break;
                case '\0': s += '\0'; ++i; break;
                default:
                    throw std::runtime_error{"not implemented"};
                }
            } else {
                s += *i;
            }
        }
        return s;
    }
    operator vector<string>() const {
        auto &p = std::get<array>(value);
        vector<string> v;
        v.reserve(p.size());
        std::ranges::copy(p, std::back_inserter(v));
        return v;
    }
    operator string_view() const {
        return std::get<string_view>(std::get<simple_value>(value));
    }
    operator vector<string_view>() const {
        auto &p = std::get<array>(value);
        vector<string_view> v;
        v.reserve(p.size());
        std::ranges::copy(p, std::back_inserter(v));
        return v;
    }

    static void check_null(auto p) {
        if (!*p) {
            throw std::runtime_error{"unexpected eof"};
        }
    }
    static void eat_space(auto &p) {
        while (*p && isspace(*p)) {
            ++p;
        }
    }
    static auto get_symbol(auto &p) {
        eat_space(p);
        return *p;
    }
    static void eat_symbol(auto &p, auto c) {
        eat_space(p);
        if (*p != c) {
            throw std::runtime_error{"unexpected '"s + c + "'"s};
        }
        ++p;
        eat_space(p);
    }
    static auto eat_string(auto &p) {
        auto start = p;
        while (!(*p == '\"' && *(p - 1) != '\\')) {
            ++p;
        }
        return string_view{start, p};
    }
    static auto eat_string_quoted(auto &p) {
        eat_symbol(p, '\"');
        auto s = eat_string(p);
        eat_symbol(p, '\"');
        return s;
    }

    template <typename T, auto start_sym, auto end_sym>
    static auto parse(auto &p, auto &&f) {
        eat_symbol(p, start_sym);
        T v;
        while (*p != end_sym) {
            f(v);
            if (*p == ',') {
                ++p;
            }
            eat_space(p);
        }
        eat_symbol(p, end_sym);
        return v;
    }
    static json parse(auto &&p) {
        eat_space(p);
        // check_null(p);
        switch (*p) {
        case '{':
            return {parse<object, '{', '}'>(p, [&](auto &&v) {
                auto key = eat_string_quoted(p);
                eat_symbol(p, ':');
                v.emplace(key, parse(p));
            })};
        case '[':
            return {parse<array, '[', ']'>(p, [&](auto &&v) {
                v.emplace_back(parse(p));
            })};
        case '\"':
            return {eat_string_quoted(p)};
        // null, true, false, int, double, ...
        default:
            throw std::runtime_error{"not implemented"};
        }
        return {};
    }
};

} // namespace sw
