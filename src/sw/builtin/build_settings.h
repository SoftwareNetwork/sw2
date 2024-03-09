// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2022 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include "os.h"
#include "../command.h"
#include "../sys/arch.h"

namespace sw {

struct empty_environment {};

struct environment {
    // path - existing program
    // unresolved_package_name or dependency with bs? in case if build os is different from host os
    // other alias key
    using alias_key = string;
    using alias_type = types<alias_key, string_view, path
        //, unresolved_package_name
    >;

    // like ccflags = ...
    // istring?
    std::map<string, string> variables;
    // rename? aliases? programs?
    std::map<alias_key, alias_type::variant_type> aliases;

    string resolve(const string &key) {
        auto it = aliases.find(key);
        return it != aliases.end() ? resolve(it->second) : key;
    }
    string resolve(const string_view &key) {
        string s{key.begin(), key.end()};
        return resolve(s);
    }
    string resolve(const path &key) {
        SW_UNIMPLEMENTED;
    }
    string resolve(const variant<string,string_view,path> &key) {
        return visit(key, [&](auto &&s) {
            return resolve(s);
        });
    }
};

struct build_settings_base {
    using os_type = special_variant<os::linux, os::macos, os::windows, os::mingw, os::cygwin, os::wasm>;
    using build_type_type = special_variant<build_type::debug, build_type::minimum_size_release,
                                            build_type::release_with_debug_information, build_type::release>;
    using library_type_type = special_variant<library_type::static_, library_type::shared>;

    os_type os;
    arch_type arch;
    build_type_type build_type;
    library_type_type library_type;

    auto for_each_hash() const {
        // same types wont work and will give wrong results
        // i.e. library_type and runtimes
        return std::tie(os, arch, build_type, library_type);
    }
    auto for_each() const {
        return for_each_hash();
    }
    auto for_each_hash(auto &&f) const {
        std::apply(
            [&](auto &&...args) {
                (f(FWD(args)), ...);
            },
            for_each_hash());
    }
    auto for_each(auto &&f) const {
        std::apply(
            [&](auto &&...args) {
                (f(FWD(args)), ...);
            },
            for_each());
    }

    template <typename T>
    bool is1() const {
        int cond = 0;
        for_each([&](auto &&a) {
            if (cond == 1) {
                return;
            }
            if constexpr (contains<T>((std::decay_t<decltype(a)> **)nullptr)) {
                cond += std::holds_alternative<T>(a);
            }
        });
        return cond == 1;
    }
    template <typename T, typename... Types>
    bool is() const {
        return (is1<T>() && ... && is1<Types>());
    }

    void visit(auto &&...f) {
        for_each([&](auto &&a) {
            visit_any(a, FWD(f)...);
        });
    }

    size_t hash() const {
        size_t h = 0;
        build_settings_base::for_each_hash([&](auto &&a) {
            ::sw::visit(a, [&](auto &&v) {
                h = hash_combine(h, v.name);
            });
        });
        return h;
    }

    auto operator<(const build_settings_base &rhs) const {
        return for_each_hash() < rhs.for_each_hash();
    }
};

struct dependency_base {
    unresolved_package_name name;
    build_settings_base bs;

    size_t hash() const {
        auto h = name.hash();
        return h;
    }

    auto operator<(const dependency_base &rhs) const {
        return std::tie(name, bs) < std::tie(rhs.name, rhs.bs);
    }
};

struct build_settings : build_settings_base {
    //using c_compiler_type = special_variant<c_compiler::clang, c_compiler::clang_cl, c_compiler::gcc, c_compiler::msvc>;
    //using cpp_compiler_type = special_variant<cpp_compiler::clang, cpp_compiler::clang_cl, cpp_compiler::gcc, cpp_compiler::msvc>;
    //using librarian_type = special_variant<librarian::ar, librarian::msvc>;
    //using linker_type = special_variant<linker::gcc, linker::gpp, linker::msvc>;

    std::vector<dependency_base> forced_dependencies;
    environment env; // rename to environment?

    /*template <typename CompilerType>
    struct native_language {
        CompilerType compiler;
        // optional?
        // vector is needed to provide winsdk.um, winsdk.ucrt
        // c++ lib and c++ rt lib (exceptions)
        std::vector<unresolved_package_name> stdlib; // can be mingw64 etc. can be libc++ etc.
        library_type_type runtime;                   // move this flag into dependency settings
    };

    // optional?
    unresolved_package_name kernel_lib; // can be winsdk.um, mingw64, linux kernel etc.
    native_language<c_compiler_type> c;
    native_language<cpp_compiler_type> cpp;
    librarian_type librarian;
    linker_type linker;*/

    auto for_each_hash() const {
        return std::tie(os, arch, build_type, library_type, forced_dependencies);
    }
    auto for_each() const {
        return for_each_hash();
    }
    auto for_each_hash(auto &&f) const {
        std::apply(
            [&](auto &&...args) {
                (f(FWD(args)), ...);
            },
            for_each_hash());
    }
    auto for_each(auto &&f) const {
        std::apply(
            [&](auto &&...args) {
                (f(FWD(args)), ...);
            },
            for_each());
    }

    size_t hash() const {
        size_t h = 0;
        build_settings_base::for_each_hash([&](auto &&a) {
            ::sw::visit(a, [&](auto &&v) {
                h = hash_combine(h, v.name);
            });
        });
        for (auto &&d : forced_dependencies) {
            h = hash_combine(h, d.hash());
        }
        return h;
    }

    auto operator<(const build_settings &rhs) const {
        return for_each() < rhs.for_each();
    }
};

} // namespace sw
