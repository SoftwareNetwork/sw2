// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2022 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include "os.h"

namespace sw {

struct build_settings_base {
    template <typename... Types>
    struct special_variant : variant<any_setting, Types...> {
        using base = variant<any_setting, Types...>;
        using base::base;
        using base::operator=;

        auto operator<(const special_variant &rhs) const {
            return base::index() < rhs.base::index();
        }
        // auto operator==(const build_settings &) const = default;

        template <typename T>
        bool is() const {
            constexpr auto c = contains<T, Types...>();
            if constexpr (c) {
                return std::holds_alternative<T>(*this);
            }
            return false;
        }

        auto for_each(auto &&f) {
            (f(Types{}), ...);
        }

        decltype(auto) visit(auto &&...args) const {
            return ::sw::visit(*this, FWD(args)...);
        }
        decltype(auto) visit_any(auto &&...args) const {
            return ::sw::visit_any(*this, FWD(args)...);
        }
        // name visit special or?
        decltype(auto) visit_no_special(auto &&...args) {
            return ::sw::visit(*this, FWD(args)..., [](any_setting &) {
            });
        }
        decltype(auto) visit_no_special(auto &&...args) const {
            return ::sw::visit(*this, FWD(args)..., [](const any_setting &) {
            });
        }
    };

    using os_type = special_variant<os::linux, os::macos, os::windows, os::mingw, os::cygwin, os::wasm>;
    using arch_type = special_variant<arch::x86, arch::x64, arch::arm, arch::arm64>;
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
    auto for_each_hash(auto &&f) const {
        std::apply(
            [&](auto &&...args) {
                (f(FWD(args)), ...);
            },
            for_each_hash());
    }
};

struct dependency_base {
    unresolved_package_name name;
    build_settings_base bs;
};

struct build_settings : build_settings_base {
    using c_compiler_type = special_variant<c_compiler::clang, c_compiler::clang_cl, c_compiler::gcc, c_compiler::msvc>;
    using cpp_compiler_type = special_variant<cpp_compiler::clang, cpp_compiler::clang_cl, cpp_compiler::gcc, cpp_compiler::msvc>;
    using librarian_type = special_variant<librarian::ar, librarian::msvc>;
    using linker_type = special_variant<linker::gcc, linker::gpp, linker::msvc>;

    template <typename CompilerType>
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
    linker_type linker;

    auto for_each_hash() const {
        // same types wont work and will give wrong results
        // i.e. library_type and runtimes
        return std::tie(os, arch, build_type, library_type, c.compiler, c.runtime, cpp.compiler, cpp.runtime);
    }
    auto for_each() const {
        // same types wont work and will give wrong results
        // i.e. library_type and runtimes
        return std::tie(os, arch, build_type, library_type, c.compiler
            //, c.runtime
            , cpp.compiler
            //, cpp.runtime
        );
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

    void visit(auto &&...f) {
        for_each([&](auto &&a) {
            visit_any(a, FWD(f)...);
        });
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

    size_t hash() const {
        size_t h = 0;
        for_each_hash([&](auto &&a) {
            /*if (a.valueless_by_exception()) {
                return;
            }*/
            ::sw::visit(a, [&](auto &&v) {
                h = hash_combine(h, v.name);
            });
        });
        return h;
    }

    auto operator<(const build_settings &rhs) const {
        return for_each() < rhs.for_each();
    }
};

} // namespace sw
