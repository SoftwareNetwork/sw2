#pragma once

#include "rule_target.h"
#include "command.h"
#include "input.h"
#include "package_id.h"
#include "helpers/xml.h"

namespace sw {

struct target_map {
    struct target_version {
        using targets_type = std::map<build_settings, target_uptr>;

        targets_type targets;
        entry_point ep;

        auto empty() const { return targets.empty(); }
        bool emplace(const package_id &id, target_uptr ptr) {
            auto it = targets.find(id.settings);
            if (it == targets.end()) {
                targets.emplace(id.settings, std::move(ptr));
                return true;
            } else {
                return false;
            }
        }
        auto &container() {
            return targets;
        }
        bool contains(const package_id &id) const {
            return targets.contains(id.settings);
        }

        auto &load(auto &s, const build_settings &bs) {
            auto it = targets.find(bs);
            if (it == targets.end()) {
                ep(s, bs);
                it = targets.find(bs);
            }
            if (it == targets.end()) {
                throw std::runtime_error{"target was not loaded with provided settings"};
            }
            return it->second;
        }
        auto try_load(auto &s, const build_settings &bs) {
            auto it = targets.find(bs);
            if (it == targets.end()) {
                ep(s, bs);
                it = targets.find(bs);
            }
            return it;
        }
    };
    struct target_versions {
        using versions_type = std::map<package_version, target_version>;

        versions_type versions;

        auto &operator[](const package_version &version) {
            auto it = versions.find(version);
            if (it == versions.end()) {
                auto &&[it2, _] = versions.emplace(version, target_version{});
                it = it2;
            }
            return it->second;
        }
        auto &container() { return versions; }

        auto find(const package_version_range &r) {
            return std::max_element(versions.begin(), versions.end(), [&](auto &&v1, auto &&v2) {
                return r.contains(v1.first) && r.contains(v2.first) && v1.first < v2.first;
            });
        }
    };

    using packages_type = std::map<package_path, target_versions>;
    packages_type packages;

    auto &operator[](const package_name &name) {
        auto it = packages.find(name.path);
        if (it == packages.end()) {
            auto &&[it2, _] = packages.emplace(name.path, target_versions{});
            it = it2;
        }
        return it->second[name.version];
    }
    bool emplace(const package_id &id, target_uptr ptr) {
        return operator[](id.name).emplace(id, std::move(ptr));
    }
    bool contains(const package_id &id) {
        return operator[](id.name).contains(id);
    }
    auto &container() { return packages; }

    auto &find(const unresolved_package_name &pkg) {
        auto it = packages.find(pkg.path);
        if (it == packages.end()) {
            throw std::runtime_error{"cannot load package: "s + string{pkg.path} + ": not found"};
        }
        auto it2 = it->second.find(pkg.range);
        if (it2 == it->second.container().end()) {
            throw std::runtime_error{"cannot load package: "s + string{pkg} + ": not found"};
        }
        return it2->second;
    }
    template <typename T>
    auto &find_first(const unresolved_package_name &pkg) {
        auto &&t = find(pkg);
        if (t.empty()) {
            // logic error?
            SW_UNIMPLEMENTED;
            //throw std::runtime_error{"cannot load package: "s + string{pkg} + ": not found"};
        }
        return *std::get<uptr<T>>(t.targets.begin()->second);
    }
    auto &find_and_load(auto &&s, const unresolved_package_name &pkg, const build_settings &bs) {
        auto it = packages.find(pkg.path);
        if (it == packages.end()) {
            throw std::runtime_error{"cannot load package: "s + string{pkg.path} + ": not found"};
        }
        auto &r = pkg.range;
        auto &cnt = it->second.container();
        for (auto &&[v,d] : cnt | std::views::reverse) {
            if (r.contains(v)) {
                auto it = d.try_load(s, bs);
                if (it != d.container().end()) {
                    return it->second;
                }
            }
        }
        throw std::runtime_error{"target was not loaded with provided settings"};
    }

    struct end_sentinel{};
    struct iterator {
        target_map &tm;
        packages_type::iterator p;
        target_versions::versions_type::iterator v;
        target_version::targets_type::iterator t;

        iterator(target_map &tm) : tm{tm} {
            init(tm,p,v,t);
        }
        iterator(target_map &tm, packages_type::iterator p) : tm{tm}, p{p} {
        }
        auto operator*() {
            struct package_id_ref {
                const package_path &path;
                const package_version &version;
                const build_settings &settings;
            };
            struct pair {
                package_id_ref ref;
                target_uptr &ptr;
            };
            return pair{package_id_ref{p->first,v->first,t->first}, t->second};
        }
        void operator++() {
            next(tm,p,v,t);
        }
        bool operator==(const iterator &rhs) const {
            return std::tie(p,v,t) == std::tie(rhs.p,rhs.v,rhs.t);
        }
        bool operator==(end_sentinel) const {
            return p == tm.container().end();
        }
        bool init(auto &&obj, auto &&it, auto && ... tail) {
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
    auto begin() { return iterator{*this}; }
    auto end() { return end_sentinel{}; }
};

struct solution {
    abspath work_dir;
    abspath binary_dir;
    const build_settings host_settings_;
    // current, per loaded package data
    path source_dir;
    const build_settings *bs{nullptr};
    // internal data
    target_map targets;
    //target_map predefined_targets; // or system targets
    std::vector<input_with_settings> inputs;
    bool dry_run = false;
    input_with_settings current_input;
    std::vector<target_uptr> temp_targets;

    //solution() {}
    solution(const abspath &binary_dir, const build_settings &host_settings) : work_dir{binary_dir}, binary_dir{binary_dir}, host_settings_{host_settings} {
    }

    template <typename T, typename... Args>
    T &add(const package_name &name, Args &&...args) {
        package_id id{name, *bs};
        if (targets.contains(id)) {
            throw std::runtime_error{format("target already exists: {}", (string)name)};
        }
        // msvc bug? without upt it converts to basic type
        std::unique_ptr<T> ptr;
        try {
            ptr = std::make_unique<T>(*this, name, FWD(args)...);
        } catch (std::exception &e) {
            throw std::runtime_error{"cannot create target: "s + e.what()};
        }
        auto &t = *ptr;
        if (dry_run) {
            targets[name].ep = current_input.ep;
            temp_targets.emplace_back(std::move(ptr));
        } else {
            targets.emplace(id, std::move(ptr));
        }
        return t;
    }
    template <typename T, typename... Args>
    T &addTarget(const package_path &name, const package_version &v) {
        return add<T>(package_name{name,v});
    }

    void gather_entry_points(const input_with_settings &i) {
        current_input = i;
        current_input(*this);
    }

    void add_entry_point(const package_name &n, entry_point &&ep) {
        targets[n].ep = std::move(ep);
    }
    void add_input(const input_with_settings &i) {
        inputs.push_back(i);
    }

    auto &load_target(const unresolved_package_name &pkg, const build_settings &bs) {
        try {
            return targets.find_and_load(*this, pkg, bs);
        } catch (std::exception &e) {
            throw std::runtime_error{(string)pkg + ": " + e.what()};
        }
    }
    auto &host_settings() const {
        return host_settings_;
    }

    enum class stage_type {
        start,
        inputs_loaded,
        prepared,
        built,
    };
    stage_type stage{};

    void load_inputs() {
        if (stage >= stage_type::inputs_loaded) {
            return;
        }
        stage = stage_type::inputs_loaded;
        for (auto &&i : inputs) {
            i(*this);
        }
    }
    void prepare() {
        for (auto &&[id, t] : targets) {
            visit(t, [&](auto &&vp) {
                auto &v = *vp;
                if constexpr (requires { v.prepare_no_deps(v, *this); }) {
                    v.prepare_no_deps(v, *this);
                }
            });
        }
        for (auto &&[id,t] : targets) {
            visit(t, [&](auto &&vp) {
                auto &v = *vp;
                if constexpr (requires { v.prepare(); }) {
                    v.prepare();
                }
                if constexpr (requires { v.prepare(v); }) {
                    v.prepare(v);
                }
            });
        }
    }
    auto make_command_executor(bool with_tests = false) {
        load_inputs();
        prepare();

        command_executor ce;
        for (auto &&[id, t] : targets) {
            visit(t, [&](auto &&vp) {
                auto &v = *vp;
                ce += v.commands;
                if (with_tests) {
                    if constexpr (requires { v.tests; }) {
                        ce += v.tests;
                    }
                }
            });
        }
        return ce;
    }
    void build(auto &&cl) {
        executor ex;
        build(ex, cl);
    }
    void build(auto &&ex, auto &&cl) {
        auto ce = make_command_executor();
        ce.ex_external = &ex;
        ce.run(cl, *this);

        if (!ce.errors.empty()) {
            string t;
            for (auto &&cmd : ce.errors) {
                visit(*cmd, [&](auto &&c) {
                    t += c.get_error_message() + "\n";
                });
            }
            t += "Total errors: " + std::to_string(ce.errors.size());
            throw std::runtime_error{t};
        }
        //return ce;
    }
    void test(auto &&cl) {
        executor ex;
        test(ex, cl);
    }
    auto test(auto &&ex, auto &&cl) {
        auto ce = make_command_executor(true);
        ce.ex_external = &ex;
        ce.ignore_errors = (std::numeric_limits<decltype(ce.ignore_errors)>::max)(); // (msvc)(prank)
        ce.run(cl, *this);
        return ce;
    }
};
using Solution = solution; // v1 compat

} // namespace sw
