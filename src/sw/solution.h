#pragma once

#include "command/command.h"
#include "command/executor.h"
#include "helpers/xml.h"
#include "input.h"
#include "package_id.h"
#include "rule_target.h"
#include "system.h"

namespace sw {

// struct system;

template <typename K, typename V>
struct settings_map {
  using targets_type = std::map<K, V>;

  targets_type targets;
  entry_point ep;

  auto empty() const { return targets.empty(); }
  bool emplace(const package_id &id, V ptr) {
    auto it = targets.find(id.settings);
    if (it == targets.end()) {
      targets.emplace(id.settings, std::move(ptr));
      return true;
    } else {
      return false;
    }
  }
  bool contains(const package_id &id) const { return targets.contains(id.settings); }
  auto &load(auto &s, const K &bs) {
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
  auto try_load(auto &s, const K &bs) {
    auto it = targets.find(bs);
    if (it == targets.end()) {
      ep(s, bs);
      it = targets.find(bs);
    }
    return it;
  }

  auto begin() { return targets.begin(); }
  auto end() { return targets.end(); }
};

struct target_map : package_map<settings_map<build_settings, target_uptr>> {
  using base = package_map<settings_map<build_settings, target_uptr>>;

  bool contains(const package_id &id) { return operator[](id.name).contains(id); }
  bool emplace(const package_id &id, target_uptr ptr) { return operator[](id.name).emplace(id, std::move(ptr)); }
  auto &find_and_load(auto &&s, const unresolved_package_name &pkg, const build_settings &bs) {
    auto it = packages.find(pkg.path);
    if (it == packages.end()) {
      throw std::runtime_error{"cannot load package: "s + string{pkg.path} + ": not found"};
    }
    auto &r = pkg.range;
    auto &cnt = it->second;
    for (auto &&[v, d] : cnt | std::views::reverse) {
      if (r.contains(v)) {
        auto it = d.try_load(s, bs);
        if (it != d.end()) {
          return it->second;
        }
      }
    }
    throw std::runtime_error{"target was not loaded with provided settings: "s + (string)pkg};
  }

  struct end_sentinel {};
  struct iterator {
    struct package_id_ref {
      const base::iterator::package_id_ref &id;
      const build_settings &settings;
    };
    struct pair {
      package_id_ref first;
      target_uptr &second;
    };

    target_map *tm;
    base::iterator p{*tm};
    mapped_type::versions_type::mapped_type::targets_type::iterator t;

    iterator(target_map &tm) : tm{&tm} { init((base &)*this->tm, p, t); }
    auto operator*() { return pair{package_id_ref{(*p).first, t->first}, t->second}; }
    auto &operator++() {
      next((base &)*tm, p, t);
      return *this;
    }
    bool operator==(const iterator &rhs) const { return std::tie(p, t) == std::tie(rhs.p, rhs.t); }
    bool operator==(end_sentinel) const { return p == tm->base::end(); }
    bool init(auto *&obj, auto &&it, auto &&...tail) { return init(*obj, it, tail...); }
    bool init(auto &&obj, auto &&it, auto &&...tail) {
      it = obj.begin();
      if constexpr (sizeof...(tail) > 0) {
        while (it != obj.end() && !init(it->second, FWD(tail)...)) {
          ++it;
        }
      }
      return it != obj.end();
    }
    bool next(auto *&obj, auto &&it, auto &&...tail) { return next(*obj, it, tail...); }
    bool next(auto &&obj, auto &&it, auto &&...tail) {
      // using T = std::decay_t<decltype(obj)>;
      if constexpr (sizeof...(tail) > 0) {
        if (!next(it->second, FWD(tail)...)) {
          ++it;
          while (it != obj.end() && !init(it->second, FWD(tail)...)) {
            ++it;
          }
          return it != obj.end();
        }
        return true;
      }
      /*if constexpr (std::is_pointer_v<T>) {
          return ++it != obj->end();
      } else {
          return ++it != obj.end();
      }*/
      return ++it != obj.end();
    }
  };
  auto begin() { return iterator{*this}; }
  auto end() { return end_sentinel{}; }
};

struct solution {
  system &sys;
  abspath work_dir;
  abspath binary_dir;
  const build_settings host_settings_;
  // current, per loaded package data
  path source_dir;
  const build_settings *bs{nullptr};
  // internal data
  target_map targets;
  // target_map predefined_targets; // or system targets
  std::vector<input_with_settings> inputs;
  bool dry_run = false;
  input_with_settings current_input;
  std::vector<target_uptr> temp_targets;

  // solution() {}
  solution(system &sys, const abspath &binary_dir, const build_settings &host_settings)
      : sys{sys}, work_dir{binary_dir}, binary_dir{binary_dir}, host_settings_{host_settings} {}

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
    return add<T>(package_name{name, v});
  }

  void gather_entry_points(const input_with_settings &i) {
    current_input = i;
    SW_UNIMPLEMENTED;
    // current_input(*this);
  }

  void add_entry_point(const package_name &n, entry_point &&ep) { targets[n].ep = std::move(ep); }
  void add_input(const input_with_settings &i) { inputs.push_back(i); }

  auto &load_target(const unresolved_package_name &pkg, const build_settings &bs) {
    try {
      return targets.find_and_load(*this, pkg, bs);
    } catch (std::exception &e) {
      throw std::runtime_error{(string)pkg + ": " + e.what()};
    }
  }
  auto &host_settings() const { return host_settings_; }

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
      SW_UNIMPLEMENTED;
      // i(*this);
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
    for (auto &&[id, t] : targets) {
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
    ce.check_errors();
    // return ce;
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
