// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2022 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include "unix.h"
#include "win32.h"

#include "../helpers/json.h"
#include "../sys/log.h"

#ifdef __GNUC__
// not yet in libstdc++
template <>
struct std::formatter<std::chrono::seconds> : formatter<std::string> {
    auto format(const std::chrono::seconds &p, format_context &ctx) const {
        return std::formatter<std::string>::format(std::format("{}", p.count()), ctx);
    }
};
#endif

namespace sw {

struct raw_command;
struct io_command;

struct command_pointer_holder {
    raw_command *r;
    io_command *io;
};

struct command_hash {
    struct hasher {
        auto operator()(const command_hash &h) const {
            return h.h;
        }
    };

    uint64_t h{0};

    void operator()(auto &&cmd) {
        h = 0;
        for (auto &&a : cmd.arguments) {
            visit(a, [&]<typename T>(const T &s) {
                h ^= std::hash<T>()(s);
            });
        }
        h ^= std::hash<path>()(cmd.working_directory);
        for (auto &&[k, v] : cmd.environment) {
            h ^= std::hash<string>()(k);
            h ^= std::hash<string>()(v);
        }
        if (auto p = std::get_if<path>(&cmd.in.s)) {
            h ^= std::hash<path>()(*p);
        }
        if (auto p = std::get_if<path>(&cmd.out.s)) {
            h ^= std::hash<path>()(*p);
        }
        if (auto p = std::get_if<path>(&cmd.err.s)) {
            h ^= std::hash<path>()(*p);
        }
    }
    explicit operator bool() const { return h; }
    operator auto() const { return h; }
    auto operator<=>(const command_hash &) const = default;
};

struct io_command;

struct command_storage {
    using clock = std::chrono::system_clock;
    using time_point = clock::time_point;
    using hash_type = command_hash;
    struct raw_tag{};

    struct not_outdated {};
    struct not_recorded_file {};
    struct new_command { const io_command *c; };
    struct new_file { const path *p; };
    struct missing_file { const path *p; };
    // struct not_regular_file {};
    struct updated_file { const path *p; };
    using outdated_reason = variant<not_outdated, new_command, new_file, not_recorded_file, missing_file, updated_file>;

    struct command_data {
        time_point mtime;
        //io_command::hash_type hash;
        std::unordered_set<uint64_t> files;
    };
    struct file_storage {
        struct file {
            path f;
            mutable time_point mtime{};
            bool checked{false};
            bool exists{false};

            void check() {
                // GetFileAttributesExW
                auto s = fs::status(f);
                exists = fs::exists(s);
                if (exists) {
                    auto lwt = fs::last_write_time(f);
#ifdef _MSC_VER
                    mtime = std::chrono::clock_cast<std::chrono::system_clock>(lwt);
#else
                    //mtime = lwt;
                    //mtime = std::chrono::clock_cast<std::chrono::system_clock>(lwt);
#if defined(__MINGW32__) || defined(__APPLE__)
                    mtime = decltype(lwt)::clock::to_sys(lwt);
#else
                    mtime = decltype(lwt)::clock::to_sys(lwt);
#endif
#endif
                }
                checked = true;
            }
            outdated_reason is_outdated(const time_point &command_time) {
                if (!checked) {
                    check();
                }
                if (!exists) {
                    return missing_file{&f};
                }
                /*if (!fs::is_regular_file(s)) {
                    std::cerr << "outdated: not regular file" << "\n";
                    return true;
                }*/
    #ifdef _MSC_VER
                if (mtime > command_time) {
    #else
                if (mtime.time_since_epoch() > command_time.time_since_epoch()) {
    #endif
                    return updated_file{&f};
                }
                return {};
            }
        };
        using hash_type = uint64_t;

        std::unordered_map<hash_type, file> files;

        file_storage() {
            files.reserve(100'000);
        }
        void add(auto &&f, auto &&files_stream, auto &&fs, bool reset) {
            auto [it, _] = files.emplace(std::hash<path>()(f), f);
            auto [_2, inserted2] = fs.insert(&it->second);
            if (inserted2) {
                files_stream << f;
            }
            if (reset) {
                it->second.checked = false;
            }
        }
        void read(auto &&files_stream, auto &&fs) {
            path f;
            while (files_stream && files_stream >> f) {
                auto h = std::hash<decltype(f)>()(f);
                auto [it,_] = files.emplace(h, f);
                fs.insert(&it->second);
            }
        }
        outdated_reason is_outdated(hash_type fh, const time_point &command_time) const {
            auto it = global_fs.files.find(fh);
            if (it == global_fs.files.end()) {
                return not_recorded_file{};
            }
            return it->second.is_outdated(command_time);
        }
    };

    using mmap_type = mmap_file<>;

    mmap_type f_commands, f_files;
    mmap_type::stream cmd_stream, files_stream;
    static inline file_storage global_fs;
    std::unordered_set<file_storage::file*> fs;
    std::unordered_map<hash_type, command_data, hash_type::hasher> commands;

    command_storage() = default;
    command_storage(const path &fn) {
        open(fn);
    }
    void open(const path &fn) {
        open1(fn / "db" / "9");
    }
    void open1(const path &fn) {
        f_commands.open(fn / "commands.bin", mmap_type::rw{});
        f_files.open(fn / "commands.files.bin", mmap_type::rw{});
        cmd_stream = f_commands.get_stream();
        files_stream = f_files.get_stream();

        if (cmd_stream.size() == 0) {
            return;
        }
        while (auto s = cmd_stream.read_record()) {
            hash_type h;
            s >> h;
            command_data v;
            s >> v.mtime;
            uint64_t n;
            s >> n;
            std::ranges::copy(s.make_span<uint64_t>(n), std::inserter(v.files, v.files.end()));
            commands[h] = v;
        }
        // f_commands.close();
        // f_commands.open(mmap_type::rw{});

        global_fs.read(files_stream, fs);
    }

    //
    outdated_reason outdated(auto &cmd, bool explain) const {
        auto h = cmd.hash();
        auto cit = commands.find(h);
        if (cit == commands.end()) {
            return new_command{&cmd};
        }
        for (auto &&f : cit->second.files) {
            if (auto r = global_fs.is_outdated(f, cit->second.mtime); !std::holds_alternative<not_outdated>(r)) {
                return r;
            }
        }
        return {};
    }
    void add(auto &&cmd) {
        uint64_t n{0};
        auto ins = [&](auto &&v, bool reset) {
            for (auto &&f : v) {
                global_fs.add(f, files_stream, fs, reset);
            }
            n += v.size();
        };
        ins(cmd.inputs, false);
        ins(cmd.implicit_inputs, false);
        ins(cmd.outputs, true);

        auto h = cmd.hash();
        auto t = *(uint64_t*)&cmd.end; // on mac it's 128 bit
        uint64_t sz = sizeof(h) + sizeof(t) + n * sizeof(h) + sizeof(n);
        auto r = cmd_stream.write_record(sz);
        r << h << t << n;
        auto write_h = [&](auto &&v) {
            for (auto &&f : v) {
                auto h = (uint64_t)std::hash<path>()(f);
                r << h;
            }
        };
        write_h(cmd.inputs);
        write_h(cmd.implicit_inputs);
        write_h(cmd.outputs);
        // flush
    }
};

// number of jobs allowed
using resource_pool = int;

struct io_command : raw_command {
    using base = raw_command;
    using clock = command_storage::clock;

    struct shell {
        struct cmd {
            static inline constexpr auto extension = ".bat";
            static inline constexpr auto prolog = R"(@echo off

setlocal

)";
            static inline constexpr auto epilog =
                R"(if %ERRORLEVEL% NEQ 0 echo Error code: %ERRORLEVEL% && exit /b %ERRORLEVEL%
)";
            static inline constexpr auto arg_delim = "^";
            static inline constexpr auto any_arg = "%*";
        };
        struct powershell {
            static inline constexpr auto extension = ".ps1";
            static inline constexpr auto prolog = R"()";
            static inline constexpr auto epilog = R"()";
            static inline constexpr auto arg_delim = "";
            static inline constexpr auto any_arg = "%*";
        };
        // needs special handling of slashes in started program names
        struct _4nt : cmd {
            static bool is_4nt() {
                auto has_str = [](auto &&env) {
                    if (auto v = getenv(env)) {
                        string s = v;
                        // this wont handle utf? but we do not have wgetenv()
                        std::transform(s.begin(), s.end(), s.begin(), ::tolower);
                        return s.contains("4nt.exe");
                    }
                    return false;
                };
                return has_str("SHELL") || has_str("ComSpec");
            }
        };
        struct sh_base {
            static inline constexpr auto extension = ".sh";
            static inline constexpr auto prolog = R"(#!/bin/sh

)";
            static inline constexpr auto epilog = R"(E=$?
if [ $E -ne 0 ]; then echo "Error code: $E"; fi
)";
            static inline constexpr auto arg_delim = "\\";
            static inline constexpr auto any_arg = "$*";
        };
        struct mingw : sh_base {
        };
        struct cygwin : sh_base {
        };
        struct sh : sh_base {
        };
    };
    using shell_type = variant<shell::cmd, shell::sh>;

    bool always{};
    std::set<path> inputs;
    std::set<path> outputs;
    std::set<path> implicit_inputs;
    mutable command_storage::hash_type h;
    command_storage::time_point start{}, end;
    command_storage *cs{};
    string name_;
    resource_pool *simultaneous_jobs{};
    bool processed{};
    //
    std::set<void*> dependencies;
    std::set<void*> dependents;
    size_t n_pending_dependencies;
    enum class dag_status { not_visited, visited, no_circle };
    dag_status dagstatus{};
    //

    auto hash() const {
        if (!h) {
            h(*this);
        }
        return h;
    }
    bool outdated(bool explain) const {
        if (always) {
            if (explain) {
                log_trace("outdated: build always");
            }
            return true;
        }
        auto explain_r = [](auto &&r) -> string {
            return visit(
                r,
                [](command_storage::not_outdated) {
                    return string{};
                },
                [](command_storage::new_command &c) {
                    return std::format("new command: hash = {}, {}", (size_t)c.c->hash(), c.c->print());
                },
                [](command_storage::new_file &f) {
                    return "new file: "s + f.p->string();
                },
                [](command_storage::not_recorded_file &) {
                    return "not recorded file"s;
                },
                [](command_storage::missing_file &f) {
                    return "missing file: "s + f.p->string();
                },
                [](command_storage::updated_file &f) {
                    return "updated file: "s + f.p->string();
                });
        };
        if (cs) {
            auto r = cs->outdated(*this, explain);
            if (std::holds_alternative<command_storage::not_outdated>(r)) {
                return false;
            }
            if (explain) {
                log_info("outdated: {}" , explain_r(r));
            }
            //if (auto ch = std::get_if<command_pointer_holder>(&out.s)) {
                //return ch->io->outdated(explain);
            //}
        }
        return true;
    }

    void operator<(const path &p) {
        base::operator<(p);
        inputs.insert(p);
    }
    void operator>(const path &p) {
        base::operator>(p);
        outputs.insert(p);
    }

    // this operator will run commands
    struct command_pipeline {
        std::vector<std::function<void(executor&)>> commands;
        raw_command *last;

        command_pipeline(auto &l, auto &r) {
            add(l);
            add(r);
        }
        ~command_pipeline() noexcept(false) {
            executor ex;
            for (auto &&c : commands) {
                c(ex);
            }
            ex.run();
        }
        void add(auto &v) {
            commands.emplace_back([&](auto &ex){
                v.run(ex);
            });
            last = &v;
        }
        command_pipeline &&operator|(auto &c) {
            last->out.s = &c.in;
            c.in.s = &last->out;
            add(c);
            return std::move(*this);
        }
    };
    auto operator|(this auto &&self, auto &&c) {
        self.out.s = &c.in;
        c.in.s = &self.out;
        command_pipeline p{self, c};
        return p;
    }
    // this operator will make a pipe between commands
    void operator|=(auto &c) {
        out.s = &c.in;
        c.in.s = &out;
    }

    string name() const {
        if (!name_.empty()) {
            return name_;
        }
        if (!outputs.empty()) {
            string s = "generating: ";
            for (auto &&o : outputs) {
                s += std::format("\"{}\", ", o.string().c_str());
            }
            s.resize(s.size() - 2);
            return s;
        }
        return print();
    }
    string get_error_message() {
        if (ok()) {
            return {};
        }
        return "command failed: " + name() + ":\n" + raw_command::get_error_message();
    }
    void save(const path &dir, shell_type t = detect_shell()) {
        auto fn = dir / std::to_string(hash()) += visit(t,[](auto &&v){return v.extension;});
        string s;
        s += visit(t,[](auto &&v){return v.prolog;});
        s += "echo " + name() + "\n\n";
        // env
        if (!working_directory.empty()) {
            s += "cd \"" + working_directory.string() + "\"\n\n";
        }
        auto start_space = "    "s;
        for (int i = 0; auto &&a : arguments) {
            if (i++) {
                s += start_space;
            }
            auto quote = [&](const std::string &as) {
                s += "\"" + as + "\" " + visit(t,[](auto &&v){return v.arg_delim;}) + "\n";
                /*if (as.contains(" ")) {
                    s += "\"" + as + "\" ";
                } else {
                    s += as + " ";
                }*/
            };
            visit(a, overload{[&](const string &s) {
                                  quote(s);
                              },
                              [&](const string_view &s) {
                                  quote(string{s.data(), s.size()});
                              },
                              [&](const path &p) {
                                  quote(p.string());
                              }});
        }
        if (!arguments.empty()) {
            s.resize(s.size() - 2 - string{visit(t,[](auto &&v){return v.arg_delim;})}.size());
            s += " "s + visit(t,[](auto &&v){return v.any_arg;});
            if (auto p = std::get_if<path>(&in.s)) {
                s += " "s + visit(t,[](auto &&v){return v.arg_delim;}) + "\n";
                s += start_space + "< " + p->string();
            }
            if (auto p = std::get_if<path>(&out.s)) {
                s += " "s + visit(t,[](auto &&v){return v.arg_delim;}) + "\n";
                s += start_space + "> " + p->string();
            }
            if (auto p = std::get_if<path>(&err.s)) {
                s += " "s + visit(t,[](auto &&v){return v.arg_delim;}) + "\n";
                s += start_space + "2> " + p->string();
            }
            s += "\n";
            s += "\n";
        }
        s += visit(t,[](auto &&v){return v.epilog;});
        write_file(fn, s);
        fs::permissions(fn, fs::status(fn).permissions() | fs::perms::owner_exec);
    }
    static shell_type detect_shell() {
        // depends on target (build) os?
#ifdef _WIN32
        return io_command::shell::cmd{};
#else
        return io_command::shell::sh{};
#endif
    }

    /*struct command_result {
        bool success{};

        command_result() = default;
        command_result(const decltype(exit_code) &e) : success{e ? *e == 0 : false} {}

        operator bool() const {return success;}
    };*/
    bool operator()(this auto &&self) {
        executor ex;
        self.run(ex, []{});
        ex.run();
        return self.exit_code ? *self.exit_code == 0 : false;
    }
};

struct cl_exe_command : io_command {
    bool old_includes{false};

    void run(auto &&ex, auto &&cb) {
        err = ""s;

        if (1 || old_includes) {
            add("/showIncludes");
            scope_exit se{[&] {
                arguments.pop_back();
            }};
            out = [&, line = 0](auto sv) mutable {
                static const auto msvc_prefix = [&]() -> string {
                    path base = fs::temp_directory_path() / "sw_msvc_prefix";
                    base = fs::absolute(base);
                    base = base.lexically_normal();
                    auto fc = path{base} += ".c";
                    auto fh = path{base} += ".h";
                    auto fo = path{base} += ".obj";

                    std::ofstream{fh};
                    std::ofstream{fc.fspath()} << "#include \"sw_msvc_prefix.h\"\nint dummy;";

                    scope_exit se{[&] {
                        fs::remove(fh);
                        fs::remove(fc);
                        fs::remove(fo);
                    }};

                    raw_command c;
                    c.environment = environment;
                    c += arguments[0];
                    c += "/nologo";
                    c += "/c";
                    c += fc;
                    c += "/showIncludes";
                    c += L"\"/Fo"s + fo.wstring() + L"\"";

                    c.out = c.err = ""s;

                    c.run();

                    auto &str = std::get<string>(c.out.s).empty() ? std::get<string>(c.err.s) : std::get<string>(c.out.s);
                    if (auto p1 = str.find("\n"); p1 != -1) {
                        while (0
                            || str[p1] == '\n'
                            || str[p1] == '\r'
                            || str[p1] == '\t'
                            || str[p1] == ' '
                            ) {
                            ++p1;
                        }
                        if (auto p2 = str.find(fh.root_path().string().c_str(), p1); p2 != -1) {
                            return str.substr(p1, p2 - p1);
                        }
                    }
                    throw std::runtime_error{"cannot find msvc prefix: "s + str};
                }();

                if (++line == 1) {
                    return;
                }
                size_t p = sv.find(msvc_prefix);
                if (p == -1) {
                    out_text += sv;
                    out_text += "\n";
                    return;
                }
                while ((p = sv.find(msvc_prefix, p)) != -1) {
                    p += msvc_prefix.size();
                    p = sv.find_first_not_of(' ', p);
                    auto fn = sv.substr(p, sv.find_first_of("\r\n", p) - p);
                    implicit_inputs.insert(string{fn.data(), fn.data() + fn.size()});
                }
            };
            io_command::run(ex, cb);
        } else {
            // >= 14.27 1927 (version 16.7)
            // https://learn.microsoft.com/en-us/cpp/build/reference/sourcedependencies?view=msvc-170
            // msvc prints errors into stdout, maybe use sourceDependencies with file always?

            auto add_deps = [&](auto &&p) {
                auto j = json::parse(p);
                // version 1.1 has different path case
                // version 1.2 has all lower paths
                vector<std::string> includes = j["Data"]["Includes"];
                std::ranges::copy(includes, std::inserter(implicit_inputs, implicit_inputs.end()));
            };

            if (0 && !outputs.empty()) {
                auto depsfile = path{*outputs.begin()} += ".json";
                add("/sourceDependencies");
                add(depsfile);
                scope_exit se{[&] {
                    arguments.pop_back();
                    arguments.pop_back();
                }};
                io_command::run(ex, [&, depsfile, add_deps, cb]() {
                    cb(); // before processing (start another process)

                    bool time_not_set = start == decltype(start){};
                    if (exit_code || time_not_set) {
                        return;
                    }

                    mmap_file<char> f{depsfile};
                    add_deps(f.p);
                });
            } else {
                out = ""s;
                add("/sourceDependencies-");
                scope_exit se{[&] {
                    arguments.pop_back();
                }};
                io_command::run(ex, [&, add_deps, cb]() {
                    cb(); // before processing (start another process)

                    bool time_not_set = start == decltype(start){};
                    if (exit_code || time_not_set) {
                        return;
                    }

                    auto &s = std::get<string>(out.s);
                    auto pos = s.find('\n');
                    add_deps(s.data() + pos + 1);
                });
            }
        }
    }
};

struct gcc_command : io_command {
    path deps_file;

    gcc_command() {
        err = ""s;
        out = ""s;
    }
    void run(auto &&ex, auto &&cb) {
        io_command::run(ex, cb);
    }
    void run(auto &&ex) {
        run(ex, [&]{process_deps();});
    }
    void process_deps() {
        auto is_space = [](auto c) {
            return c == ' ' || c == '\n' || c == '\r' || c == '\t';
        };
        auto add_file = [&](auto sv) {
            string s;
            s.reserve(sv.size());
            for (auto c : sv) {
                if (c == '\\') {
                    continue;
                }
                s += c;
            }
            // we may have 'src/../sw.h' or 'sw4/./sw.h' paths
            // call absolute? or lexi normal?
            path p = s;
#ifndef _WIN32
            p = p.lexically_normal();
#endif
            implicit_inputs.insert(p);
        };

        if (deps_file.empty()) {
            return;
        }

        mmap_file<char> f{deps_file};
        if (f.sz == 0) {
            throw std::runtime_error{std::format("cannot open deps file: {}", deps_file)};
        }
        string_view sv{f.p, f.sz};
        auto p = sv.find(": ");
        if (p == -1) {
            throw std::runtime_error{"bad deps file"};
        }
        //auto outputs = sv.substr(0, p);
        auto inputs = sv.substr(p + 2);
        while (!inputs.empty()) {
            if (isspace(inputs[0]) || inputs[0] == '\\') {
                inputs = inputs.substr(1);
                continue;
            }
            for (int i = 0; auto &c : inputs) {
                if (is_space(c) && *(&c-1) != '\\') {
                    add_file(inputs.substr(0, i));
                    inputs = inputs.substr(i);
                    goto next;
                }
                ++i;
            }
            add_file(inputs);
            next:;
        }
    }
};

using command = variant<io_command, cl_exe_command, gcc_command>;

} // namespace sw
