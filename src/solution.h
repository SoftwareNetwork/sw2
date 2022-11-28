#pragma once

#include "rule_target.h"
#include "os.h"
#include "input.h"
#include "package_id.h"
#include "build_settings.h"

namespace sw {

struct target_map {
    struct target_version {
        using targets_type = std::map<build_settings, target_uptr>;

        targets_type targets;
        entry_point ep;

        bool emplace(const package_id &id, target_uptr ptr) {
            auto it = targets.find(id.settings);
            if (it == targets.end()) {
                targets.emplace(id.settings, std::move(ptr));
                return true;
            } else {
                return false;
            }
        }
        auto &container() { return targets; }

        auto &load(auto &s, const build_settings &bs) {
            auto it = targets.find(bs);
            if (it == targets.end()) {
                visit(ep, [&](auto &&v){v(s, bs);});
            }
            it = targets.find(bs);
            if (it == targets.end()) {
                throw std::runtime_error{"target was not loaded with provided settings"};
            }
            return it->second;
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
            }
            return ++it != obj.container().end();
        }
    };
    auto begin() { return iterator{*this}; }
    auto end() { return end_sentinel{}; }
};

struct solution {
    abspath source_dir{"."};
    abspath binary_dir{".sw4"};
    // config

    const build_settings *bs{nullptr};

    // internal data
    target_map targets;
    target_map predefined_targets; // or system targets
    //std::vector<rule> rules;
    std::vector<input_with_settings> inputs;

    // some settings
    bool system_targets_detected{false};

    solution() {
        //rules.push_back(cl_exe_rule{});
        //rules.push_back(link_exe_rule{});
    }

    template <typename T, typename... Args>
    T &add(Args &&...args) {
        // msvc bug? without upt it converts to basic type
        auto ptr = std::make_unique<T>(*this, FWD(args)...);
        auto &t = *ptr;
        auto inserted = targets.emplace(package_id{t.name, *bs}, std::move(ptr));
        if (!inserted) {
            throw std::runtime_error{"target already exists"};
        }
        return t;
    }

    void add_entry_point(const package_name &n, source_code_input &&ep) {
        targets[n].ep = std::move(ep);
    }
    void add_input(auto &&i) {
        static const auto bs = default_build_settings();
        //inputs.emplace_back(input_with_settings{i, {bs}});
    }
    void add_input(input_with_settings &i) {
        inputs.emplace_back(i);
    }
    void add_input(const input_with_settings &i) {
        inputs.emplace_back(i);
    }

    auto &load_target(const unresolved_package_name &pkg, const build_settings &bs) {
        auto &tv = targets.find(pkg);
        return tv.load(*this, bs);
    }
    auto &host_settings() const {
        static const auto hs = default_build_settings();
        return hs;
    }

    void prepare() {
        for (auto &&[id,t] : targets) {
            visit(t, [&](auto &&vp) {
                auto &v = *vp;
                //if constexpr (std::derived_from<std::decay_t<decltype(v)>, rule_target>) {
                    //for (auto &&r : rules) {
                        //v += r;
                    //}
                //}
                if constexpr (requires { v.prepare(); }) {
                    v.prepare();
                }
            });
        }
    }
    void build() {
        executor ex;
        build(ex);
    }
    void build(auto &&ex) {
        for (auto &&i : inputs) {
            i(*this);
        }

        prepare();

        command_executor ce{ex};
        for (auto &&[id,t] : targets) {
            visit(t, [&](auto &&vp) {
                auto &v = *vp;
                if constexpr (requires { v.commands; }) {
                    ce += v.commands;
                }
            });
        }
        ce.run();
    }
};

} // namespace sw
