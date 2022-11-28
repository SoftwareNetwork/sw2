// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2022 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include "helpers.h"
#include "package.h"
#include "suffix.h"
#include "os.h"
#include "target_properties.h"

namespace sw {

struct target_base {
    package_name name;
    build_settings bs;

    target_base(const package_name &n) : name{n} {
    }
};

// binary_target_package?
struct binary_target : target_base, compile_options_t, link_options_t {
    path executable;

    binary_target(auto &&s, const package_name &n) : target_base{n} {
        bs = *s.bs;
    }
};
struct binary_library_target : target_base, compile_options_t, link_options_t {
    binary_library_target(auto &&s, const package_name &n) : target_base{n} {
        bs = *s.bs;
    }
};

struct msvc_instance;
struct binary_target_msvc : binary_target {
    const msvc_instance &msvc;

    binary_target_msvc(auto &&s, const package_name &n, auto &msvc) : binary_target{s,n}, msvc{msvc} {
    }
};

struct files_target : target_base {
    using files_t = std::set<path>; // unordered?

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
    auto operator+=(this auto &&self, auto &&v) {
        self.add(v);
        return appender{[&](auto &&v) { self.add(v); }};
    }
    auto operator-=(this auto &&self, auto &&v) {
        self.remove(v);
        return appender{[&](auto &&v) { self.remove(v); }};
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
