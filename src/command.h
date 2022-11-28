// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2022 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include "helpers.h"
#include "win32.h"
#include "linux.h"
#include "macos.h"
#include "mmap.h"
#include "json.h"

namespace sw {

struct raw_command {
    using argument = variant<string,string_view,path>;
    std::vector<argument> arguments;
    path working_directory;
    std::map<string, string> environment;

    // exit_code
    // pid, pidfd

    // stdin,stdout,stderr
    using stream_callback = std::function<void(string_view)>;
    using stream = variant<std::monostate, string, stream_callback, path>;
    //stream in;
    stream out, err;
    string out_text; // filtered, workaround
#ifdef _WIN32
    win32::pipe pout, perr;
#endif

    // sync()
    // async()

    std::wstring printw() const {
        std::wstring s;
        for (auto &&a : arguments) {
            auto quote = [&](const std::wstring &as) {
                if (as.contains(L" ")) {
                    s += L"\"" + as + L"\" ";
                } else {
                    s += as + L" ";
                }
            };
            visit(a, overload{[&](const string &s) {
                                  quote(path{s}.wstring());
                              },
                              [&](const string_view &s) {
                                  quote(path{string{s.data(), s.size()}}.wstring());
                              },
                              [&](const path &p) {
                                  quote(p.wstring());
                              }});
        }
        return s;
    }
    std::string print() const {
        std::string s;
        for (auto &&a : arguments) {
            auto quote = [&](const std::string &as) {
                if (as.contains(" ")) {
                    s += "\"" + as + "\" ";
                } else {
                    s += as + " ";
                }
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
        return s;
    }

    // execute?
    void shell_execute() {
        //ShellExecuteA(NULL, "open", "C:\\WINDOWS\\system32\\calc.exe", "/k ipconfig", 0, SW_SHOWNORMAL);
        //run_win32{}
    }
    void run_win32(auto &&ex, auto &&cb) {
#ifdef _WIN32
        DWORD flags = 0;
        STARTUPINFOEXW si = {0};
        si.StartupInfo.cb = sizeof(si);
        PROCESS_INFORMATION pi = {0};
        LPVOID env = 0;
        int inherit_handles = 1; // must be 1

        flags |= NORMAL_PRIORITY_CLASS;
        flags |= CREATE_UNICODE_ENVIRONMENT;
        flags |= EXTENDED_STARTUPINFO_PRESENT;
        // flags |= CREATE_NO_WINDOW;

        si.StartupInfo.dwFlags = STARTF_USESTDHANDLES;

        auto setup_stream = [&](auto &&s, auto &&h, auto &&stdh, auto &&pipe) {
            visit(
                s,
                [&](std::monostate) {
                    h = GetStdHandle(stdh);
                },
                [&](path &fn) {
                    SECURITY_ATTRIBUTES sa = {0};
                    sa.bInheritHandle = TRUE;
                    h = CreateFileW(fn.wstring().c_str(), GENERIC_WRITE, 0, &sa, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
                },
                [&](auto &) {
                    pipe.init(true);
                    ex.register_handle(pipe.r);
                    h = pipe.w;
                });
        };
        setup_stream(out, si.StartupInfo.hStdOutput, STD_OUTPUT_HANDLE, pout);
        setup_stream(err, si.StartupInfo.hStdError, STD_ERROR_HANDLE, perr);

        WINAPI_CALL(CreateProcessW(0, printw().data(), 0, 0,
            inherit_handles, flags, env,
            working_directory.empty() ? 0 : working_directory.wstring().c_str(),
            (LPSTARTUPINFOW)&si, &pi));
        CloseHandle(pi.hThread);

        auto post_setup_stream = [&](auto &&s, auto &&h, auto &&pipe) {
            visit(
                s,
                [&](std::monostate) {
                },
                [&](string &s) {
                    pipe.w.reset();
                    ex.read_async(pipe.r, [&](this auto &&f, auto &&buf, auto &&ec) {
                        if (!ec) {
                            s += buf;
                            ex.read_async(pipe.r, f);
                        }
                    });
                },
                [&](stream_callback &cb) {
                    pipe.w.reset();
                    ex.read_async(pipe.r, [&, cb, s = string()](this auto &&f, auto &&buf, auto &&ec) {
                        if (!ec) {
                            s += buf;
                            if (auto p = s.find_first_of("\r\n"); p != -1) {
                                cb(string_view(s.data(), p));
                                p = s.find("\n", p);
                                s = s.substr(p + 1);
                            }
                            ex.read_async(pipe.r, f);
                        }
                    });
                },
                [&](path &fn) {
                    CloseHandle(h);
                }
            );
        };
        post_setup_stream(out, si.StartupInfo.hStdOutput, pout);
        post_setup_stream(err, si.StartupInfo.hStdError, perr);

        // If we do not create default job for main process, we have a race here.
        // If main process is killed before register_process() call, created process
        // won't stop.
        ex.register_process(pi.hProcess, pi.dwProcessId, [this, cb, h = pi.hProcess]() {
            DWORD exit_code;
            GetExitCodeProcess(h, &exit_code);
            scope_exit se{[&] {
                CloseHandle(h); // we may want to use h in the callback
            }};
            cb(exit_code);
        });
#endif
    }
    void run_linux(auto &&ex, auto &&cb) {
#if defined(__linux__)
        std::vector<string> args;
        args.reserve(arguments.size());
        for (auto &&a : arguments) {
            visit(a, overload{[&](string &s) {
                args.push_back(s);
            },
                              [&](string_view &s) {
                                  args.push_back(string{s.data(), s.size()});
                              },
                              [&](path &p) {
                                  args.push_back(p.string());
                              }});
        }
        std::vector<char *> args2;
        args2.reserve(args.size() + 1);
        for (auto &&a : args) {
            args2.push_back(a.data());
        }
        args2.push_back(0);

        auto mkpipe = [](int my_pipe[2]) {
            if (pipe2(my_pipe, 0) == -1) {
                fprintf(stderr, "Error creating pipe\n");
            }
            return my_pipe;
        };
        int pout[2]; mkpipe(pout);
        int perr[2]; mkpipe(perr);

        clone_args cargs{};
        cargs.flags |= CLONE_PIDFD;
        cargs.flags |= CLONE_VFORK; // ?
        //cargs.exit_signal = SIGCHLD;
        int pidfd;
        cargs.pidfd = &pidfd;
        auto pid = clone3(&cargs, sizeof(cargs));
        if (pid == -1) {
            throw std::runtime_error{"cant clone3: "s + std::to_string(errno)};
        }
        if (pid == 0) {
            auto setup_pipe = [](auto my_pipe, auto fd) {
                //while ((dup2(my_pipe[1], STDERR_FILENO) == -1) && (errno == EINTR)) {}
                if (dup2(my_pipe[1], fd) == -1) {
                    std::cerr << "dup2 error: " << errno << "\n";
                    exit(1);
                }
                close(my_pipe[0]);
                close(my_pipe[1]);
            };
            setup_pipe(pout, STDOUT_FILENO);
            setup_pipe(perr, STDERR_FILENO);
            // child
            if (execve(args2[0], args2.data(), environ) == -1) {
                std::cerr << "execve error: " << errno << "\n";
                exit(1);
            }
        }
        auto postsetup_pipe = [&](auto my_pipe) {
            close(my_pipe[1]);
            ex.register_read_handle(my_pipe[0]);
        };
        postsetup_pipe(pout);
        postsetup_pipe(perr);
        ex.register_process(pidfd, [pid, pidfd, pout = pout[0], perr = perr[0], cb](){
            scope_exit se{[&] {
                close(pout);
                close(perr);
                close(pidfd);
            }};

            int wstatus;
            if (waitpid(pid, &wstatus, 0) == -1) {
                throw std::runtime_error{"error waitpid: " + std::to_string(errno)};
            }
            if (WIFEXITED(wstatus)) {
                int exit_code = WEXITSTATUS(wstatus);
                cb(exit_code);
            }
        });
#endif
    }
    void run_macos() {
        //
    }
    void run(auto &&ex, auto &&cb) {
#ifdef _WIN32
        run_win32(ex, cb);
#elif defined(__linux__)
        run_linux(ex, cb);
#endif
    }
    void run(auto &&ex) {
        run(ex, [](auto exit_code) {
            if (exit_code) {
                throw std::runtime_error(
                    "process exit code: " + std::to_string(exit_code));
            }
        });
    }

    void add(auto &&p) {
        arguments.push_back(p);
    }
    void add(const char *p) {
        arguments.push_back(string{p});
    }
    // TODO: exclude bools, chars
    void add(std::integral auto v) {
        add(std::to_string(v));
    }
    auto operator+=(auto &&arg) {
        add(arg);
        return appender{[&](auto &&v){add(v);}};
    }

    void operator>(const path &p) {
        out = p;
    }
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
    }
    explicit operator bool() const { return h; }
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
                    mtime = lwt;
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
                    // std::cerr << "outdated: file does not exists" << "\n";
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
                    //std::cerr << "outdated: file lwt > command time" << "\n";
                }
                return {};
            }
        };
        using hash_type = uint64_t;

        std::unordered_map<hash_type, file> files;

        file_storage() {
            files.reserve(100'000);
        }
        void add(auto &&f, auto &&files_stream) {
            auto [_, inserted] = files.emplace(std::hash<path>()(f), f);
            if (inserted) {
                files_stream << f;
            }
        }
        void read(auto &&files_stream) {
            path f;
            while (files_stream && files_stream >> f) {
                fs.files.emplace(std::hash<decltype(f)>()(f), f);
            }
        }
        outdated_reason is_outdated(hash_type fh, const time_point &command_time) const {
            auto it = fs.files.find(fh);
            if (it == fs.files.end()) {
                return not_recorded_file{};
            }
            return it->second.is_outdated(command_time);
        }
    };

    using mmap_type = mmap_file<>;

    mmap_type f_commands, f_files;
    mmap_type::stream cmd_stream, files_stream;
    static inline file_storage fs;
    std::unordered_map<hash_type, command_data, hash_type::hasher> commands;

    command_storage() : command_storage{".", raw_tag{}} {}
    command_storage(const path &fn) : command_storage{fn / "db" / "9", raw_tag{}} {
    }
    command_storage(const path &fn, raw_tag)
            : f_commands{fn / "commands.bin", mmap_type::rw{}}
            , f_files{fn / "commands.files.bin", mmap_type::rw{}}
            , cmd_stream{f_commands.get_stream()}, files_stream{f_files.get_stream()} {
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
        //f_commands.close();
        //f_commands.open(mmap_type::rw{});

        fs.read(files_stream);
    }

    //
    bool outdated(auto &&cmd) const {
        return !std::holds_alternative<not_outdated>(outdated1(cmd));
    }
    outdated_reason outdated1(auto &&cmd) const {
        auto h = cmd.hash();
        auto cit = commands.find(h);
        if (cit == commands.end()) {
            return new_command{&cmd};
            //std::cerr << "outdated: command is missing" << "\n";
        }
        for (auto &&f : cit->second.files) {
            if (auto r = fs.is_outdated(f, cit->second.mtime); !std::holds_alternative<not_outdated>(r)) {
                return r;
            }
        }
        return {};
    }
    void add(auto &&cmd) {
        uint64_t n{0};
        auto ins = [&](auto &&v) {
            for (auto &&f : v) {
                fs.add(f, files_stream);
            }
            n += v.size();
        };
        ins(cmd.inputs);
        ins(cmd.implicit_inputs);
        ins(cmd.outputs);

        auto h = cmd.hash();
        auto t = *(uint64_t*)&cmd.end; // on mac it's 128 bit
        uint64_t sz = sizeof(h) + sizeof(t) + n * sizeof(h) + sizeof(n);
        auto r = cmd_stream.write_record(sz);
        r << h << t << n;
        auto write_h = [&](auto &&v) {
            for (auto &&f : v) {
                r << (uint64_t)std::hash<path>()(f);
            }
        };
        write_h(cmd.inputs);
        write_h(cmd.implicit_inputs);
        write_h(cmd.outputs);
        // flush
    }
};

struct io_command : raw_command {
    using base = raw_command;
    using clock = command_storage::clock;

    bool always{false};
    std::set<path> inputs;
    std::set<path> outputs;
    std::set<path> implicit_inputs;
    mutable command_storage::hash_type h;
    command_storage::time_point start{}, end;
    command_storage *cs{nullptr};
    string name_;
    //
    std::set<void*> dependencies;
    std::set<void*> dependents;
    size_t n_pending_dependencies;
    enum class dag_status { not_visited, visited, no_circle };
    dag_status dagstatus{};
    //

    bool outdated() const {
        return always || cs && cs->outdated(*this);
    }
    void run(auto &&ex, auto &&cb) {
        if (!outdated()) {
            cb();
            return;
        }
        // use GetProcessTimes or similar for time
        start = clock::now();
        raw_command::run(ex, [&, cb](auto exit_code) {
            end = clock::now();
            if (exit_code) {
                string t;
                if (auto e = std::get_if<string>(&err); e && !e->empty()) {
                    t = *e;
                } else if (auto e = std::get_if<string>(&out); e && !e->empty()) {
                    t = *e;
                } else if (!out_text.empty()) {
                    t = out_text;
                }
                throw std::runtime_error(
                    //format("process exit code: {}\nerror: {}", exit_code, std::get<string>(err))
                    "process exit code: " + std::to_string(exit_code) + "\nerror: " + t + ""
                );
            }
            cb();
            if (cs) {
                cs->add(*this);
            }
        });
    }
    auto hash() const {
        if (!h) {
            h(*this);
        }
        return h;
    }

    void operator>(const path &p) {
        base::operator>(p);
        outputs.insert(p);
    }

    string name() const {
        if (!name_.empty()) {
            return name_;
        }
        if (!outputs.empty()) {
            string s = "generating: ";
            for (auto &&o : outputs)
                s += std::format("\"{}\", ", (const char *)o.u8string().c_str());
            s.resize(s.size() - 2);
            return s;
        }
        return print();
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
                    auto fc = path{base} += ".c";
                    auto fh = path{base} += ".h";
                    auto fo = path{base} += ".obj";

                    std::ofstream{fh};
                    std::ofstream{fc} << "#include \"sw_msvc_prefix.h\"\nint dummy;";

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

                    executor ex;
                    c.run(ex);
                    ex.run();

                    auto &str = std::get<string>(c.out).empty() ? std::get<string>(c.err) : std::get<string>(c.out);
                    if (auto p1 = str.find("\n"); p1 != -1) {
                        if (auto p2 = str.find((const char *)fh.u8string().c_str(), p1 + 1); p2 != -1) {
                            return str.substr(p1 + 1, p2 - (p1 + 1));
                        }
                    }
                    throw std::runtime_error{"cannot find msvc prefix"};
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
                vector<std::u8string> includes = j["Data"]["Includes"];
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
                io_command::run(ex, [&, depsfile, add_deps, cb] {
                    cb(); // before processing (start another process)

                    if (start == decltype(start){}) {
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
                io_command::run(ex, [&, add_deps, cb] {
                    cb(); // before processing (start another process)

                    if (start == decltype(start){}) {
                        return;
                    }

                    auto &s = std::get<string>(out);
                    auto pos = s.find('\n');
                    add_deps(s.data() + pos + 1);
                });
            }
        }
    }
};

struct gcc_command : io_command {
    void run(auto &&ex, auto &&cb) {
        err = ""s;
        out = ""s;

        io_command::run(ex, cb);
    }
};

using command = variant<io_command, cl_exe_command, gcc_command>;

struct command_executor {
    static void create_output_dirs(auto &&commands) {
        std::unordered_set<path> outdirs;
        for (auto &&c : commands) {
            visit(*c, [&](auto &&c) {
                for (auto &&o : c.outputs) {
                    outdirs.insert(o.parent_path());
                }
            });
        }
        for (auto &&d : outdirs) {
            fs::create_directories(d);
        }
    }
    static void make_dependencies(auto &&commands) {
        std::map<path, void *> cmds;
        for (auto &&c : commands) {
            visit(*c, [&](auto &&c1) {
                for (auto &&f : c1.outputs) {
                    auto [_, inserted] = cmds.emplace(f, c);
                    if (!inserted) {
                        throw std::runtime_error{"more than one command produces: "s + f.string()};
                    }
                }
            });
        }
        for (auto &&c : commands) {
            visit(*c, [&](auto &&c1) {
                for (auto &&f : c1.inputs) {
                    if (auto i = cmds.find(f); i != cmds.end()) {
                        c1.dependencies.insert(i->second);
                        visit(*(command *)i->second, [&](auto &&d1) {
                            d1.dependents.insert(c);
                        });
                    }
                }
                c1.n_pending_dependencies = c1.dependencies.size();
            });
        }
    }

    static void check_dag1(auto &&c) {
        if (c.dagstatus == io_command::dag_status::no_circle) {
            return;
        }
        if (c.dagstatus == io_command::dag_status::visited) {
            throw std::runtime_error{"circular dependency detected"};
        }
        c.dagstatus = io_command::dag_status::visited;
        for (auto &&d : c.dependencies) {
            visit(*(command *)d, [&](auto &&d1) {
                check_dag1(d1);
            });
        }
        c.dagstatus = io_command::dag_status::no_circle;
    }
    static void check_dag(auto &&commands) {
        // rewrite into non recursive?
        for (auto &&c : commands) {
            visit(*c, [&](auto &&c) {
                check_dag1(c);
            });
        }
    }

    std::deque<command *> pending_commands;
    int running_commands{0};
    size_t maximum_running_commands{std::thread::hardware_concurrency()};
    executor ex;
    executor *ex_external{nullptr};
    std::vector<command*> external_commands;
    int command_id{0};

    command_executor() {
        init();
    }
    command_executor(executor &ex) : ex_external{&ex} {
        init();
    }

    void init() {
#ifdef _WIN32
        // always create top level job and associate self with it
        win32::default_job_object();
#endif
    }

    auto &get_executor() {
        return ex_external ? *ex_external : ex;
    }

    void run_next() {
        if (running_commands > maximum_running_commands) {
            int a = 5;
            a++;
        }
        while (running_commands < maximum_running_commands && !pending_commands.empty()) {
            auto c = pending_commands.front();
            pending_commands.pop_front();
            visit(*c, [&](auto &&c) {
                ++running_commands;
                std::cout << std::format("[{}/{}] {}\n", ++command_id, external_commands.size(), c.name());
                c.run(get_executor(), [&] {
                    --running_commands;
                    for (auto &&d : c.dependents) {
                        visit(*(command *)d, [&](auto &&d1) {
                            if (!--d1.n_pending_dependencies) {
                                pending_commands.push_back((command *)d);
                                run_next();
                            }
                        });
                    }
                });
            });
        }
    }
    void run(auto &&tgt) {
        command_storage cs{tgt.binary_dir};
        for (auto &&c : tgt.commands) {
            c.cs = &cs;
        }
        external_commands.clear();
        *this += tgt.commands;
        run();
    }
    void run() {
        create_output_dirs(external_commands);
        make_dependencies(external_commands);
        check_dag(external_commands);

        // initial set of commands
        for (auto &&c : external_commands) {
            visit(*c, [&](auto &&c1) {
                if (c1.dependencies.empty()) {
                    pending_commands.push_back(c);
                }
            });
        }
        run_next();
        get_executor().run();
    }

    void operator+=(std::vector<command> &commands) {
        for (auto &&c : commands) {
            external_commands.push_back(&c);
        }
    }
};

}
