// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2022 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include "rule.h"
#include "os.h"
#include "target_list.h"

namespace sw {

struct dependency {
    target_uptr *target;
};

// basic target: throw files, rules etc.
struct rule_target : files_target {
    using base = files_target;

#ifdef _MSC_VER
    using base::operator+=;
    using base::operator-=;
#endif
    using base::add;
    using base::remove;

    const build_settings solution_bs;
    build_settings bs;
    path binary_dir;
    command_storage cs;
    std::vector<rule> rules;
    std::map<path, rule_flag> processed_files;
    std::vector<command> commands;
    //
    bool dry_run{};
    // v1 compat
    path &SourceDir{source_dir};
    path &BinaryDir{binary_dir};
    bool &DryRun{dry_run};

    rule_target(auto &&solution, auto &&id)
        : files_target{id}
        , solution_bs{*solution.bs}
        , bs{*solution.bs}
   {
        source_dir = solution.source_dir;
        binary_dir = make_binary_dir(solution.binary_dir);
    }
    path make_binary_dir(const path &parent) {
        return make_binary_dir(parent, bs.hash());
    }
    path make_binary_dir(const path &parent, auto &&config) {
        return parent / "t" / std::to_string(config) / std::to_string(name.hash());
    }

#ifndef _MSC_VER
    auto operator+=(auto &&v) {
        add(v);
        return appender{[&](auto &&v) { add(v); }};
    }
    auto operator-=(auto &&v) {
        remove(v);
        return appender{[&](auto &&v) { remove(v); }};
    }
#endif

    /*auto &build_settings() {
        return bs;
    }
    auto &build_settings() const {
        return bs;
    }*/
    //auto &getBuildSettings() const { return bs; } // v1 compat

    template <typename T, typename ... Types>
    bool is() { return bs.is<T, Types...>(); }
    void visit(auto && ... f) {
        bs.visit(FWD(f)...);
    }

    void add(const rule &r) {
        rules.push_back(r);
    }
    void init_rules(/*this */auto &&self) {
        for (auto &&r : self.rules) {
            std::visit([&](auto &&v){
                if constexpr (requires {v(self);}) {
                    v(self);
                }
            }, r);
            for (auto &&c : self.commands) {
                ::sw::visit(c, [&](auto &&c) {
                    for (auto &&o : c.outputs) {
                        self.processed_files[o]; // better return a list of new files from rule and add them
                    }
                });
            }
        }
        self.cs.open(self.binary_dir);
        for (auto &&c : self.commands) {
            ::sw::visit(c, [&](auto &&c2) {
                c2.cs = &self.cs;
            });
        }
    }

    void prepare() {
        init_rules(*this);
    }
    void build(/*this */auto &&self) {
        self.prepare(self);

        command_executor ce;
        ce.run(self);
    }

    //auto visit()
};

struct target_data {
    compile_options_t compile_options;
    link_options_t link_options;
    std::vector<dependency> dependencies;
};
struct target_data_storage : target_data {
    struct groups {
        enum {
            self    = 0b001,
            project = 0b010,
            others  = 0b100,
        };
    };

    // we need 2^3 = 8 values
    // minus 1 inherited
    // plus  1 merge_object value
    //
    // inherited = 0
    // array[0] = merge_object
    std::array<std::unique_ptr<target_data>, 8> data;

    target_data &private_{*this};
    target_data &protected_{get(groups::self | groups::project)};
    target_data &public_{get(groups::self | groups::project | groups::others)};
#undef interface // some win32 stuff
    target_data &interface{get(groups::project | groups::others)};
    target_data &interface_{get(groups::project | groups::others)}; // for similarity

protected:
    target_data &merge_object{get(0)};
private:
    target_data &get(int i) {
        if (!data[i]) {
            data[i] = std::make_unique<target_data>();
        }
        return *data[i];
    }
};

struct native_target : rule_target, target_data_storage {
    using base = rule_target;

#ifdef _MSC_VER
    using base::operator+=;
    using base::operator-=;
#endif
    using base::add;
    using base::remove;

    string api_name;
    string &ApiName{api_name}; // v1 compat

    native_target(auto &&s, auto &&id) : base{s, id} {
        *this += native_sources_rule{};
        init_compilers(s);

        bs.os.visit_any([&](os::windows) {
            *this += "SW_EXPORT=__declspec(dllexport)"_def;
            *this += "SW_IMPORT=__declspec(dllimport)"_def;
        });
        if (!bs.build_type.is<build_type::debug>()) {
            *this += "NDEBUG"_def;
        }
    }

    void init_compilers(auto &&s) {
        auto load = [&] {
            std::once_flag once;
            auto load = [&] {
#ifdef _WIN32
                detect_winsdk(s);
                get_msvc_detector().add(s);
                detect_gcc_clang(s);
#else
                detect_gcc_clang(s);
#endif
            };
            visit_any(
                bs.c.compiler,
                [&](c_compiler::msvc &c) {
                    std::call_once(once, load);
                },
                [&](c_compiler::gcc &c) {
                    std::call_once(once, load);
                },
                [&](c_compiler::clang &c) {
                    std::call_once(once, load);
                });
            visit_any(
                bs.cpp.compiler,
                [&](cpp_compiler::msvc &c) {
                    std::call_once(once, load);
                },
                [&](cpp_compiler::gcc &c) {
                    std::call_once(once, load);
                },
                [&](cpp_compiler::clang &c) {
                    std::call_once(once, load);
                });
            visit_any(bs.linker, [&](librarian::msvc &c) {
                std::call_once(once, load);
            });
            visit_any(bs.linker, [&](linker::msvc &c) {
                std::call_once(once, load);
            });
        };
        std::call_once(s.system_targets_detected, load);

        // order
        for (auto &&l : bs.cpp.stdlib) {
            add(s.load_target(l, bs));
        }
        for (auto &&l : bs.c.stdlib) {
            add(s.load_target(l, bs));
        }
#if defined(_WIN32)
        add(s.load_target(bs.kernel_lib, bs));
#endif

        ::sw::visit(
            bs.c.compiler,
            [&](c_compiler::msvc &c) {
                auto &t = s.load_target(c.package, bs);
                add(c_compiler::msvc::rule_type{t});
            },
            [&](c_compiler::gcc &c) {
                auto &t = s.load_target(c.package, bs);
                add(c_compiler::gcc::rule_type{t});
            },
            [&](c_compiler::clang &c) {
                auto &t = s.load_target(c.package, bs);
                add(c_compiler::clang::rule_type{t,true});
            },
            [](auto &) {
                SW_UNIMPLEMENTED;
            });
        ::sw::visit(
            bs.cpp.compiler,
            [&](cpp_compiler::msvc &c) {
                auto &t = s.load_target(c.package, bs);
                add(cpp_compiler::msvc::rule_type{t});
            },
            [&](cpp_compiler::gcc &c) {
                auto &t = s.load_target(c.package, bs);
                add(cpp_compiler::gcc::rule_type{t,false,true});
            },
            [&](cpp_compiler::clang &c) {
                auto &t = s.load_target(c.package, bs);
                add(cpp_compiler::clang::rule_type{t,true,true});
            },
            [](auto &) {
                SW_UNIMPLEMENTED;
            });
    }

#ifndef _MSC_VER
    auto operator+=(auto &&v) {
        add(v);
        return appender{[&](auto &&v) { add(v); }};
    }
    auto operator-=(auto &&v) {
        remove(v);
        return appender{[&](auto &&v) { remove(v); }};
    }
#endif

    void add(target_uptr &ptr) {
        dependencies.push_back({&ptr});
    }
    void add(const system_link_library &l) {
        link_options.system_link_libraries.push_back(l);
    }
    void add(const definition &d) {
        compile_options.definitions.push_back(d);
    }
    void add(const include_directory &d) {
        compile_options.include_directories.push_back(d);
    }
    void add(const compile_option &d) {
        compile_options.compile_options.push_back(d);
    }

    //void build() { operator()(); }
    // void run(){}

    void prepare() {
        if (!api_name.empty()) {
            *this += definition{api_name, "SW_EXPORT"};
            public_ += definition{api_name, "SW_IMPORT"};
        }
        rule_target::prepare();
    }

    bool has_file(const path &fn) {
        return false;
    }
    void check_absolute(path &fn, bool = false, bool *source_dir = nullptr) {
        if (!fn.is_absolute()) {
            fn = fs::absolute(fn);
        }
        //return true;
    }
    void add_file_silently(const path &from) {
        // add to target if not already added
        if (DryRun) {
            operator-=(from);
        } else {
            auto fr = from;
            check_absolute(fr);
            if (!has_file(fr))
                operator-=(from);
        }
    }
    void configure_file(path from, path to, int flags) {
        add_file_silently(from);

        // before resolving
        if (!to.is_absolute())
            to = BinaryDir / to;
        //File(to, getFs()).setGenerated();

        if (DryRun)
            return;

        if (!from.is_absolute()) {
            if (fs::exists(SourceDir / from))
                from = SourceDir / from;
            else if (fs::exists(BinaryDir / from))
                from = BinaryDir / from;
            else
                throw std::runtime_error("Package: " + getPackage().toString() + ", file not found: " + from.string());
        }

        // we really need ExecuteCommand here!!! or not?
        // auto c = std::make_shared<DummyCommand>();// ([this, from, to, flags]()
        { configureFile1(from, to, flags); } //);
        // c->addInput(from);
        // c->addOutput(to);

        //if ((int)flags & (int)ConfigureFlags::AddToBuild)
            //operator+=(to);
    }
    void configureFile(auto && ... args) {return configure_file(args...);} // v1 compat
    void configureFile1(const path &from, const path &to, int flags)
    {
        static const std::regex cmDefineRegex(R"xxx(#\s*cmakedefine[ \t]+([A-Za-z_0-9]*)([^\r\n]*?)[\r\n])xxx");
        static const std::regex cmDefine01Regex(R"xxx(#\s*cmakedefine01[ \t]+([A-Za-z_0-9]*)[^\r\n]*?[\r\n])xxx");
        static const std::regex mesonDefine(R"xxx(#\s*mesondefine[ \t]+([A-Za-z_0-9]*)[^\r\n]*?[\r\n])xxx");
        static const std::regex undefDefine(R"xxx(#undef[ \t]+([A-Za-z_0-9]*)[^\r\n]*?[\r\n])xxx");
        static const std::regex cmAtVarRegex("@([A-Za-z_0-9/.+-]+)@");
        static const std::regex cmNamedCurly("\\$\\{([A-Za-z0-9/_.+-]+)\\}");

        static const std::set<string> offValues{
            "", "0", //"OFF", "NO", "FALSE", "N", "IGNORE",
        };

        // ide files
        //configure_files.insert(from);

        auto s = read_file(from);

        /*if ((int)flags & (int)ConfigureFlags::CopyOnly) {
            writeFileOnce(to, s);
            return;
        }*/

        auto find_repl = [this, &from, flags](const auto &key) -> std::optional<std::string>
        {
            //auto v = Variables.find(key);
            //if (v != Variables.end())
                //return v->second.toString();

            //if ((int)flags & (int)ConfigureFlags::ReplaceUndefinedVariablesWithZeros)
                //return "0";

            return {};
        };

        std::smatch m;

        // @vars@
        while (std::regex_search(s, m, cmAtVarRegex) ||
            std::regex_search(s, m, cmNamedCurly))
        {
            auto repl = find_repl(m[1].str());
            if (!repl)
            {
                s = m.prefix().str() + m.suffix().str();
                // make additional log level for this
                //LOG_TRACE(logger, "configure @@ or ${} " << m[1].str() << ": replacement not found");
                continue;
            }
            s = m.prefix().str() + *repl + m.suffix().str();
        }

        // #mesondefine
        while (std::regex_search(s, m, mesonDefine))
        {
            auto repl = find_repl(m[1].str());
            if (!repl)
            {
                s = m.prefix().str() + "/* #undef " + m[1].str() + " */\n" + m.suffix().str();
                // make additional log level for this
                //LOG_TRACE(logger, "configure #mesondefine " << m[1].str() << ": replacement not found");
                continue;
            }
            s = m.prefix().str() + "#define " + m[1].str() + " " + *repl + "\n" + m.suffix().str();
        }

        // #undef
        if (0)//(int)flags & (int)ConfigureFlags::EnableUndefReplacements)
        {
            while (std::regex_search(s, m, undefDefine))
            {
                auto repl = find_repl(m[1].str());
                if (!repl)
                {
                    // space to prevent loops
                    s = m.prefix().str() + "/* # undef " + m[1].str() + " */\n" + m.suffix().str();
                    // make additional log level for this
                    //LOG_TRACE(logger, "configure #undef " << m[1].str() << ": replacement not found");
                    continue;
                }
                if (offValues.find(to_upper_copy(*repl)) != offValues.end())
                    // space to prevent loops
                    s = m.prefix().str() + "/* # undef " + m[1].str() + " */\n" + m.suffix().str();
                else
                    s = m.prefix().str() + "#define " + m[1].str() + " " + *repl + "\n" + m.suffix().str();
            }
        }

        // #cmakedefine
        while (std::regex_search(s, m, cmDefineRegex))
        {
            auto repl = find_repl(m[1].str());
            if (!repl)
            {
                // make additional log level for this
                //LOG_TRACE(logger, "configure #cmakedefine " << m[1].str() << ": replacement not found");
                repl = ""s;
            }
            if (offValues.find(to_upper_copy(*repl)) != offValues.end())
                s = m.prefix().str() + "/* #undef " + m[1].str() + m[2].str() + " */\n" + m.suffix().str();
            else
                s = m.prefix().str() + "#define " + m[1].str() + m[2].str() + "\n" + m.suffix().str();
        }

        // #cmakedefine01
        while (std::regex_search(s, m, cmDefine01Regex))
        {
            auto repl = find_repl(m[1].str());
            if (!repl)
            {
                // make additional log level for this
                //LOG_TRACE(logger, "configure #cmakedefine01 " << m[1].str() << ": replacement not found");
                repl = ""s;
            }
            if (offValues.find(to_upper_copy(*repl)) != offValues.end())
                s = m.prefix().str() + "#define " + m[1].str() + " 0" + "\n" + m.suffix().str();
            else
                s = m.prefix().str() + "#define " + m[1].str() + " 1" + "\n" + m.suffix().str();
        }

        write_file_once(to, s);
    }
    void write_file_once(const path &fn, const string &content) {
        bool source_dir = false;
        path p = fn;
        check_absolute(p, true, &source_dir);
        if (!fs::exists(p)) {
            if (!p.is_absolute()) {
                p = BinaryDir / p;
                source_dir = false;
            }
        }

        // before resolving, we must set file as generated, to skip it on server
        // only in bdir case
        if (!source_dir) {
            //File f(p, getFs());
            //f.setGenerated();
        }

        if (DryRun)
            return;

        ::sw::write_file_once(p, content, get_patch_dir(!source_dir));

        add_file_silently(p);
    }
    void writeFileOnce(auto && ... args) { write_file_once(args...); }

    path get_patch_dir(bool binary_dir) const {
        path base;
        //if (auto d = getPackage().getOverriddenDir(); d)
            //base = d.value() / SW_BINARY_DIR;
        //else if (!isLocal())
            //base = getPackage().getDirSrc();
            base = source_dir;
        //else
            //base = getMainBuild().getBuildDirectory();
        return base / "patch";
    }
};

struct native_library_target : native_target {
    using base = native_target;
    path library;
    path implib;

    native_library_target(auto &&s, auto &&id, std::optional<bool> shared = {}) : base{s, id} {
        if (!shared) {
            shared = bs.library_type.visit(
                [](const library_type::static_ &){
                    return false;
                },
                [](const library_type::shared &) {
                    return true;
                }, [](auto &) {
                    return true; // for now shared if the default
                }
            );
        }
        if (*shared) {
            bs.library_type = library_type::shared{};
        } else if (!*shared) {
            bs.library_type = library_type::static_{};
        }
        binary_dir = make_binary_dir(s.binary_dir);

        if (is<library_type::shared>()) {
            ::sw::visit(
                bs.linker,
                [&](linker::msvc &c) {
                    auto &t = s.load_target(c.package, bs);
                    add(linker::msvc::rule_type{t});
                },
                [](auto &) {
                    SW_UNIMPLEMENTED;
                });

            library = binary_dir / "bin" / (string)name;
            implib = binary_dir / "lib" / (string)name;
            ::sw::visit(bs.os, [&](auto &&os) {
                if constexpr (requires { os.shared_library_extension; }) {
                    library += os.shared_library_extension;
                }
                if constexpr (requires { os.shared_library_extension; }) {
                    implib += os.static_library_extension;
                }
            });
            bs.os.visit_any([&](os::windows) {
                *this += "_WINDLL"_def;
            });
        } else {
            ::sw::visit(
                bs.librarian,
                [&](librarian::msvc &c) {
                    auto &t = s.load_target(c.package, bs);
                    add(librarian::msvc::rule_type{t});
                },
                [](auto &) {
                    SW_UNIMPLEMENTED;
                });

            library = binary_dir / "lib" / (string)name;
            ::sw::visit(bs.os, [&](auto &&os) {
                if constexpr (requires { os.static_library_extension; }) {
                    library += os.static_library_extension;
                }
            });
        }
    }
};
using LibraryTarget = native_library_target; // v1 compat

struct native_shared_library_target : native_library_target {
    using base = native_library_target;
    native_shared_library_target(auto &&s, auto &&id) : base{s, id, true} {
    }
};
struct native_static_library_target : native_library_target {
    using base = native_library_target;
    native_static_library_target(auto &&s, auto &&id) : base{s, id, false} {
    }
};

struct executable_target : native_target {
    using base = native_target;
    path executable;

    executable_target(auto &&s, auto &&id) : base{s, id} {
        executable = binary_dir / "bin" / (string)name;
        ::sw::visit(bs.os, [&](auto &&os) {
            if constexpr (requires {os.executable_extension;}) {
                executable += os.executable_extension;
            }
        });
        ::sw::visit(
            bs.linker,
            [&](linker::msvc &c) {
                auto &t = s.load_target(c.package, bs);
                add(linker::msvc::rule_type{t});
            },
            [&](linker::gcc &c) {
                auto &t = s.load_target(c.package, bs);
                add(linker::gcc::rule_type{t});
            },
            [&](linker::gpp &c) {
                auto &t = s.load_target(c.package, bs);
                add(linker::gpp::rule_type{t});
            },
            [](auto &) {
                SW_UNIMPLEMENTED;
            });
    }

    void run(auto && ... args) {
        // make rule?
    }
};
using executable = executable_target;

using target = target_type::variant_type;

} // namespace sw
