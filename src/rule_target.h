// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2022 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include "build_settings.h"
#include "target_list.h"
#include "target_properties.h"
#include "suffix.h"

namespace sw {

struct dependency {
    unresolved_package_name name;
    target_uptr *target = nullptr;

    auto resolved() const { return !!target; }
};
auto operator""_dep(const char *s, size_t len) {
    return dependency{std::string{s, len}};
}

struct native_sources_rule {
    void operator()(auto &&tgt) const requires requires {tgt.merge_object();} {
        for (auto &&f : tgt.merge_object()) {
            if (is_cpp_file(f) || is_c_file(f)) {
                tgt.processed_files[f].insert(this);
            }
        }
    }
};

using rule_flag_type = size_t;
using rule_type = std::function<void(const target_ptr &)>;

struct rule_flag {
    std::set<rule_flag_type> rules;

    template <typename T>
    static auto get_rule_tag() {
        static void *p;
        return (rule_flag_type)&p;
    }
    template <typename T>
    bool contains(T &&) {
        return rules.contains(get_rule_tag<std::remove_cvref_t<std::remove_pointer_t<T>>>());
    }
    template <typename T>
    auto insert(T &&) {
        return rules.insert(get_rule_tag<std::remove_cvref_t<std::remove_pointer_t<T>>>());
    }
};

template <typename T>
auto make_rule(T rule, auto &&f) {
    rule_flag_type tag;
    if constexpr (requires {rule.hash();}) {
        tag = rule.hash();
    } else {
        tag = rule_flag::get_rule_tag<T>();
    }
    rule_type fun = [f](auto &&var) mutable {
        std::visit(
            [&](auto &&v) mutable {
                f(*v);
            },
            var);
    };
    return std::pair{tag, fun};
}

// basic target: throw files, rules etc.
struct rule_target {
    package_name name;
    const build_settings solution_bs;
    build_settings bs;
    path source_dir;
    path binary_dir;
    //path solution_binary_dir;
    command_storage cs;
    std::map<path, rule_flag> processed_files;
    std::vector<command> commands;
    //
    bool dry_run{};
    // v1 compat
    path &SourceDir{source_dir};
    path &BinaryDir{binary_dir};
    bool &DryRun{dry_run};

    rule_target(auto &&solution, auto &&id)
        : name{id}
        , solution_bs{*solution.bs}
        , bs{*solution.bs}
   {
        source_dir = solution.source_dir;
        binary_dir = make_binary_dir(solution.binary_dir);
        //solution_binary_dir = solution.binary_dir;
    }
    path make_binary_dir(const path &parent) {
        return make_binary_dir(parent, bs.hash());
    }
    path make_binary_dir(const path &parent, auto &&config) {
        return parent / "t" / std::to_string(config) / std::to_string(name.hash());
    }

    auto &get_package() const { return name; }
    auto &getPackage() const { return get_package(); } // v1 compat

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

    void init_rules(/*this */auto &&self) {
        while (1) {
            auto sz = self.commands.size();
            for (auto &&[_, r]: self.merge_object().rules) {
                if constexpr (requires { r(&self); }) {
                    r(&self);
                }
                for (auto &&c: self.commands) {
                    ::sw::visit(c, [&](auto &&c) {
                        for (auto &&o: c.outputs) {
                            self.processed_files[o]; // better return a list of new files from rule and add them
                        }
                    });
                }
            }
            if (sz == self.commands.size()) {
                break;
            }
        }
        self.cs.open(self.binary_dir);
        for (auto &&c : self.commands) {
            ::sw::visit(c, [&](auto &&c2) {
                c2.cs = &self.cs;
            });
        }
    }

    void prepare(/*this */auto &&self) {
        self.init_rules(self);
    }
    void build(/*this */auto &&self) {
        self.prepare(self);

        command_executor ce;
        ce.run(self);
    }

    //auto visit()
};

template <typename T>
struct target_data : compile_options_t,link_options_t {
    using files_t = std::set<path>; // unordered?

    T *target_{nullptr};
    files_t files;
    std::vector<dependency> dependencies;
    std::vector<std::pair<rule_flag_type, rule_type>> rules;

    target_data() {}
    target_data(T &t) : target_{&t} {
    }

    T &target() {
        return *target_;
    }
    const T &target() const {
        return *target_;
    }

#ifdef _MSC_VER
    auto operator+=(this auto &&self, auto &&v) {
        self.add(v);
        return appender{[&](auto &&v) {
            self.add(v);
        }};
    }
    auto operator-=(this auto &&self, auto &&v) {
        self.remove(v);
        return appender{[&](auto &&v) {
            self.remove(v);
        }};
    }
#else
    auto operator+=(auto &&v) {
        add(v);
        return appender{[&](auto &&v) {
            add(v);
        }};
    }
    auto operator-=(auto &&v) {
        remove(v);
        return appender{[&](auto &&v) {
            remove(v);
        }};
    }
#endif

    void add(const file_regex &r) {
        r(target().source_dir, [&](auto &&iter) {
            for (auto &&e : iter) {
                if (fs::is_regular_file(e)) {
                    add(e);
                }
            }
        });
    }
    void add(const path &p) {
        files.insert(p.is_absolute() ? p : target().source_dir / p);
    }
    void remove(const file_regex &r) {
        r(target().source_dir, [&](auto &&iter) {
            for (auto &&e : iter) {
                if (fs::is_regular_file(e)) {
                    remove(e);
                }
            }
        });
    }
    void remove(const path &p) {
        files.erase(p.is_absolute() ? p : target().source_dir / p);
    }

    void add(const definition &d) {
        definitions.push_back(d);
    }
    void add(const include_directory &d) {
        include_directories.push_back(d);
    }
    void add(const compile_option &d) {
        compile_options.push_back(d);
    }
    void add(const system_link_library &l) {
        system_link_libraries.push_back(l);
    }
    void add(const decltype(rules)::value_type &r) {
        if (std::ranges::find_if(rules, [&](auto &&v){return v.first == r.first;}) == rules.end()) {
            rules.push_back(r);
        }
    }
    void add(target_uptr &ptr) {
        dependency d;
        d.target = &ptr;
        add(d);
    }
    void add(const dependency &d) {
        dependencies.push_back(d);
    }

    void merge(auto &&from) {
        files.merge(from.files);
        compile_options_t::merge(from);
        link_options_t::merge(from);
        append_vector(dependencies, from.dependencies);
        for (auto &&p : from.rules) {
            add(p);
        }
    }

    auto range() const {
        return files | std::views::transform([&](auto &&p) {
            return target().source_dir / p;
        });
    }
    auto begin() const {
        return iter_with_range{range()};
    }
    auto end() const {
        return files.end();
    }
};
template <typename T>
struct target_data_storage : target_data<T> {
    using base = target_data<T>;

    struct groups {
        enum {
            self    = 0b001,
            project = 0b010,
            others  = 0b100,

            max  = 8,
        };
    };

    // we need 2^3 = 8 values
    // minus 1 inherited
    // plus  1 merge_object value
    //
    // inherited = 1
    // array[0] = merge_object
    std::array<std::unique_ptr<base>, groups::max> data;

    base &private_{*this};
    base &protected_{get(groups::self | groups::project)};
    base &public_{get(groups::self | groups::project | groups::others)};
#undef interface // some win32 stuff
    base &interface{get(groups::project | groups::others)}; // remove?
    base &interface_{get(groups::project | groups::others)}; // for similarity? remove?
    // base &interface_no_project{get(groups::others)};

    // v1 compat
    base &Private{private_};
    base &Protected{protected_};
    base &Public{public_};
    base &Interface{interface_};

    target_data_storage(T &t) : base{t} {
    }

    base &merge_object() { return merge_object_; }

    void merge_from_deps() {
    }
    void merge(auto &&from, int grps) {
        for (int i = 2; i < groups::max; ++i) {
            if (from.data[i] && (i & grps)) {
                merge_object().merge(*from.data[i]);
            }
        }
    }
    void merge() {
        // make merge object with pointers?

        // merge self
        merge_object().merge(*this);
        // merge others
        merge(*this, groups::self);
        // merge our deps
        for (auto &&d : merge_object().dependencies) {
            if (!d.target) {
                continue;
            }
            visit(*d.target, [&](auto &&v) {
                if constexpr (requires { v->data; }) {
                    // TODO: handle project correctly
                    //merge(*v, groups::project);
                    merge(*v, groups::others);
                }
            });
        }
    }

protected:
    base &merge_object_{get(0)};
private:
    base &get(int i) {
        if (!data[i]) {
            data[i] = std::make_unique<base>();
            data[i]->target_ = base::target_;
        }
        return *data[i];
    }
};

struct native_target : rule_target, target_data_storage<native_target> {
    using base = rule_target;

    struct raw_target_tag {};

#ifdef _MSC_VER
    using target_data_storage::operator+=;
    using target_data_storage::operator-=;
#endif
    using target_data_storage::add;
    using target_data_storage::remove;
    using target_data_storage::begin;
    using target_data_storage::end;
    //using rule_target::add;

    string api_name;
    string &ApiName{api_name}; // v1 compat

    native_target(auto &&s, auto &&id) : native_target{s, id, raw_target_tag{}} {
        add(make_rule(native_sources_rule{}, [&](auto &&) {
            for (auto &&f : merge_object()) {
                if (is_cpp_file(f) || is_c_file(f)) {
                    processed_files[f].insert(this);
                }
            }
        }));

        init_compilers(s);

        bs.os.visit_any([&](os::windows) {
            *this += "SW_EXPORT=__declspec(dllexport)"_def;
            *this += "SW_IMPORT=__declspec(dllimport)"_def;
        });
        if (!bs.build_type.is<build_type::debug>()) {
            *this += "NDEBUG"_def;
        }

        // unexistent, cmake cannot idir such dirs, it requires to create them
        // create during cmake project export?
        *this += include_directory{local_binary_private_dir()};
        public_ += include_directory{local_binary_dir()};
    }
    native_target(auto &&s, auto &&id, raw_target_tag) : base{s, id}, target_data_storage<native_target>{*this} {
    }

    void init_compilers(auto &&s) {
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

        bs.c.compiler.visit_no_special([&](auto &c) {
            add(s.load_target(c.package, bs));
        });
        bs.cpp.compiler.visit_no_special([&](auto &c) {
            add(s.load_target(c.package, bs));
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

    //void build() { operator()(); }
    // void run(){}

    void prepare_no_deps(/*this */ auto &&self, auto &&sln) {
        if (!api_name.empty()) {
            *this += definition{api_name, "SW_EXPORT"};
            public_ += definition{api_name, "SW_IMPORT"};
        }
        merge();
        //sln.load_target(solution_bs);
    }
    void prepare(/*this */auto &&self) {
        rule_target::prepare(self);
    }

    path local_binary_dir() {
        return binary_dir / "bd";
    }
    path local_binary_private_dir() {
        return binary_dir / "bdp";
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
    void configure_file(path from, path to, int flags = 0) {
        add_file_silently(from);

        auto bdir = local_binary_private_dir();

        // before resolving
        if (!to.is_absolute())
            to = bdir / to;
        //File(to, getFs()).setGenerated();

        if (DryRun)
            return;

        if (!from.is_absolute()) {
            if (fs::exists(SourceDir / from))
                from = SourceDir / from;
            else if (fs::exists(bdir / from))
                from = bdir / from;
            else
                throw std::runtime_error{"Package: "s + getPackage().toString() + ", file not found: " + from.string()};
        }

        { configureFile1(from, to, flags); }

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
        // for binary dir put into binary dir

        path base;
        //if (auto d = getPackage().getOverriddenDir(); d)
            //base = d.value() / SW_BINARY_DIR;
        //else if (!isLocal())
            //base = getPackage().getDirSrc();
            base = source_dir;
        //else
            //base = getMainBuild().getBuildDirectory();

        base = this->binary_dir;
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
            bs.linker.visit_no_special([&](auto &c) {
                add(s.load_target(c.package, bs));
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
            bs.librarian.visit_no_special([&](auto &c) {
                add(s.load_target(c.package, bs));
            });

            library = binary_dir / "lib" / (string)name;
            ::sw::visit(bs.os, [&](auto &&os) {
                if constexpr (requires { os.static_library_extension; }) {
                    library += os.static_library_extension;
                }
            });
        }
    }
    native_library_target(auto &&s, auto &&id, raw_target_tag t) : base{s, id, t} {
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
        bs.linker.visit_no_special([&](auto &c) {
            add(s.load_target(c.package, bs));
        });
    }
    executable_target(auto &&s, auto &&id, raw_target_tag t) : base{s, id, t} {
    }

    void run(auto && ... args) {
        // make rule?
    }
};
using executable = executable_target;

using target = target_type::variant_type;

} // namespace sw
