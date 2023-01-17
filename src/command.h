// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2022 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include "helpers/common.h"
#include "sys/win32.h"
#include "sys/linux.h"
#include "sys/macos.h"
#include "sys/mmap.h"
#include "helpers/json.h"
#include "sys/log.h"

namespace sw {

struct raw_command;
struct io_command;

struct command_pointer_holder {
    raw_command *r;
    io_command *io;
};

struct raw_command {
    using argument = variant<string,string_view,path>;
    std::vector<argument> arguments;
    path working_directory;
    std::map<string, string> environment;

    bool started{}; // state?
    std::optional<int> exit_code;
    // pid, pidfd

    // stdin,stdout,stderr
    using stream_callback = std::function<void(string_view)>;
    struct inherit {};
    using stream = variant<inherit, string, stream_callback, path, command_pointer_holder>;
    stream in, out, err;
    string out_text; // filtered, workaround
#ifdef _WIN32
    struct process_data {
        STARTUPINFOEXW si = {0};
        PROCESS_INFORMATION pi = {0};
        win32::handle thread, process;
        win32::pipe pin, pout, perr;

        process_data() {
            si.StartupInfo.cb = sizeof(si);
        }
        void close() {
            //CloseHandle(si.StartupInfo.hStdInput);
            //CloseHandle(si.StartupInfo.hStdOutput);
            //CloseHandle(si.StartupInfo.hStdError);
            *this = process_data{};
        }
    };
    process_data d;
#endif

    // sync()
    // async()

    auto printw() const {
        std::wstring p;
        std::wstring s;
        for (auto &&a : arguments) {
            auto quote = [&](const std::wstring &as) {
                std::wstring t;
                if (p.empty()) {
                    auto t = as;
                    std::replace(t.begin(), t.end(), L'/', L'\\');
                    p = t;
                }
                if (as.contains(L" ")) {
                    t += L"\"" + as + L"\" ";
                } else {
                    t += as + L" ";
                }
                s += t;
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
        return std::tuple{p,s};
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
        LPVOID env = 0;// GetEnvironmentStringsW();
        int inherit_handles = 1; // must be 1, we pass handles in proc attributes
        std::vector<HANDLE> handles;
        handles.reserve(10);

        flags |= NORMAL_PRIORITY_CLASS;
        flags |= CREATE_UNICODE_ENVIRONMENT;
        flags |= EXTENDED_STARTUPINFO_PRESENT;
        // flags |= CREATE_NO_WINDOW;

        d.si.StartupInfo.dwFlags = STARTF_USESTDHANDLES;

        visit(
            in,
            [&](inherit) {
                // we should copy this only for raw commands
                //si.StartupInfo.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
            },
            [&](path &fn) {
                SECURITY_ATTRIBUTES sa = {0};
                sa.bInheritHandle = TRUE;
                d.si.StartupInfo.hStdInput = WINAPI_CALL_HANDLE(CreateFileW(fn.wstring().c_str(), GENERIC_READ, 0, &sa, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0));
            },
            [&](command_pointer_holder &ch) {
                d.si.StartupInfo.hStdInput = ch.r->d.pout.r;
            },
            [&](auto &) {
                d.pin.init_read(true);
                ex.register_handle(d.pin.w);
                d.si.StartupInfo.hStdInput = d.pin.r;
            });
        auto setup_stream = [&](auto &&s, auto &&h, auto &&stdh, auto &&pipe) {
            visit(
                s,
                [&](inherit) {
                    h = GetStdHandle(stdh);
                },
                [&](path &fn) {
                    SECURITY_ATTRIBUTES sa = {0};
                    sa.bInheritHandle = TRUE;
                    h = WINAPI_CALL_HANDLE(CreateFileW(fn.wstring().c_str(), GENERIC_WRITE, 0, &sa, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0));
                },
                [&](command_pointer_holder &) {
                    pipe.init_write_double();
                    h = pipe.w;
                },
                [&](auto &) {
                    pipe.init_write(true);
                    ex.register_handle(pipe.r);
                    h = pipe.w;
                });
        };
        setup_stream(out, d.si.StartupInfo.hStdOutput, STD_OUTPUT_HANDLE, d.pout);
        setup_stream(err, d.si.StartupInfo.hStdError, STD_ERROR_HANDLE, d.perr);

        auto mingw = is_mingw_shell();
        auto add_handle = [&](auto &&h) {
            if (h) {
                 // mingw uses same stdout and stderr, so if we pass two same handles they are give an error 87
                //if (mingw) {
                // we always check for same handles because it can happen not only on mingw
                    if (std::ranges::find(handles, h) != std::end(handles)) {
                        return;
                    }
                //}
                handles.push_back(h);
            }
        };
        add_handle(d.si.StartupInfo.hStdInput);
        add_handle(d.si.StartupInfo.hStdOutput);
        add_handle(d.si.StartupInfo.hStdError);
        SIZE_T size = 0;
        InitializeProcThreadAttributeList(0, 1, 0, &size);
        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
            throw std::runtime_error{"InitializeProcThreadAttributeList()"};
        }
        d.si.lpAttributeList = (LPPROC_THREAD_ATTRIBUTE_LIST)HeapAlloc(GetProcessHeap(), 0, size);
        if (!d.si.lpAttributeList) {
            throw std::runtime_error{"cannot alloc GetProcessHeap()"};
        }
        scope_exit seh{[&] {
            HeapFree(GetProcessHeap(), 0, d.si.lpAttributeList);
        }};
        WINAPI_CALL(InitializeProcThreadAttributeList(d.si.lpAttributeList, 1, 0, &size));
        scope_exit sel{[&]{
            DeleteProcThreadAttributeList(d.si.lpAttributeList);
        }};
        WINAPI_CALL(UpdateProcThreadAttribute(d.si.lpAttributeList, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, handles.data(),
                                              handles.size() * sizeof(HANDLE), 0, 0));

        auto [prog,cmd] = printw();
        auto wdir = working_directory.wstring();
        WINAPI_CALL(CreateProcessW(prog.data(), cmd.data(), 0, 0,
            inherit_handles, flags, env,
            wdir.empty() ? 0 : wdir.data(),
            (LPSTARTUPINFOW)&d.si, &d.pi));
        d.thread = d.pi.hThread;
        d.process = d.pi.hProcess;

        visit(
            in,
            [&](inherit) {
            },
            [&](string &s) {
                SW_UNIMPLEMENTED;
                /*pin.r.reset();
                ex.write_async(pin.w, [&](auto &&f, auto &&buf, auto &&ec) mutable {
                    if (!ec) {
                        s += buf;
                        ex.write_async(pin.w, std::move(f));
                    }
                });*/
            },
            [&](command_pointer_holder &) {
                //CloseHandle(d.si.StartupInfo.hStdInput);
            },
            [&](stream_callback &cb) {
                SW_UNIMPLEMENTED;
                /*pin.r.reset();
                ex.write_async(pin.w, [&, cb, s = string{}](auto &&f, auto &&buf, auto &&ec) mutable {
                    if (!ec) {
                        s += buf;
                        if (auto p = s.find_first_of("\r\n"); p != -1) {
                            cb(string_view(s.data(), p));
                            p = s.find("\n", p);
                            s = s.substr(p + 1);
                        }
                        ex.write_async(pin.w, std::move(f));
                    }
                });*/
            },
            [&](path &fn) {
                CloseHandle(d.si.StartupInfo.hStdInput);
            });
        auto post_setup_stream = [&](auto &&s, auto &&h, auto &&pipe) {
            visit(
                s,
                [&](inherit) {
                },
                [&](string &s) {
                    pipe.w.reset();
                    ex.read_async(pipe.r, [&](auto &&f, auto &&buf, auto &&ec) mutable {
                        if (!ec) {
                            s += buf;
                            ex.read_async(pipe.r, std::move(f));
                        }
                    });
                },
                [&](command_pointer_holder &) {
                   //CloseHandle(h);
                },
                [&](stream_callback &cb) {
                    pipe.w.reset();
                    ex.read_async(pipe.r, [&, cb, s = string{}](auto &&f, auto &&buf, auto &&ec) mutable {
                        if (!ec) {
                            s += buf;
                            if (auto p = s.find_first_of("\r\n"); p != -1) {
                                cb(string_view(s.data(), p));
                                p = s.find("\n", p);
                                s = s.substr(p + 1);
                            }
                            ex.read_async(pipe.r, std::move(f));
                        }
                    });
                },
                [&](path &fn) {
                    CloseHandle(h);
                }
            );
        };
        post_setup_stream(out, d.si.StartupInfo.hStdOutput, d.pout);
        post_setup_stream(err, d.si.StartupInfo.hStdError, d.perr);

        // If we do not create default job for main process, we have a race here.
        // If main process is killed before register_process() call, created process
        // won't stop.
        ex.register_process(d.pi.hProcess, d.pi.dwProcessId, [this, cb]() {
            DWORD exit_code;
            WINAPI_CALL(GetExitCodeProcess(d.pi.hProcess, &exit_code));
            while (exit_code == STILL_ACTIVE) {
                WINAPI_CALL(GetExitCodeProcess(d.pi.hProcess, &exit_code));
            }
            this->exit_code = exit_code;

            //d.close_file_handles();

            //scope_exit se{[&] {
                //CloseHandle(pi.hProcess); // we may want to use h in the callback?
                finish();
            //}};
            cb();
        });
        visit(
            out,
            [&](command_pointer_holder &ch) {
                ch.r->run(ex, cb);
            },
            [&](auto &) {
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
        int pin[2] = {-1,-1};
        int pout[2] = {-1,-1};
        int perr[2] = {-1,-1};

        visit(
                in,
                [&](inherit) {
                    // nothing, inheritance is by default?
                },
                [&](path &fn) {
                    mkpipe(pin);
                    auto fd = open(fn.string().c_str(), O_RDONLY);
                    if (fd == -1) {
                        throw std::runtime_error(format("cannot open file for reading: {}", fn.string()));
                    }
                    if (dup2(pin[1], fd) == -1) {
                        std::cerr << "dup2 error: " << errno << "\n";
                        exit(1);
                    }
                    close(pin[1]);
                },
                [&](auto &) {
                    mkpipe(pin);
                });
        auto setup_stream = [&](auto &&s, auto &&pipe) {
            visit(
                    s,
                    [&](inherit) {
                        // nothing, inheritance is by default
                    },
                    [&](path &fn) {
                        mkpipe(pipe);
                        auto fd = open(fn.string().c_str(), O_CREAT | O_WRONLY | O_TRUNC);
                        if (fd == -1) {
                            throw std::runtime_error(format("cannot open file for writing: {}", fn.string()));
                        }
                        if (dup2(fd, pipe[0]) == -1) {
                            std::cerr << "dup2 error: " << errno << "\n";
                            exit(1);
                        }
                        close(fd);
                    },
                    [&](auto &) {
                        mkpipe(pipe);
                    });
        };
        setup_stream(out, pout);
        setup_stream(err, perr);

        clone_args cargs{};
        cargs.flags |= CLONE_PIDFD;
        cargs.flags |= CLONE_VFORK; // ?
        //cargs.exit_signal = SIGCHLD;
        int pidfd;
        cargs.pidfd = &pidfd;
        auto pid = clone3(&cargs, sizeof(cargs));
        if (pid == -1) {
            throw std::runtime_error{"can't clone3: "s + std::to_string(errno)};
        }
        if (pid == 0) {
            visit(
                    in,
                    [&](inherit) {
                        // we should close this only for io commands
                        close(STDIN_FILENO);
                    },
                    [&](auto &) {
                        // while ((dup2(pipe[1], STDERR_FILENO) == -1) && (errno == EINTR)) {}
                        if (dup2(pin[0], STDIN_FILENO) == -1) {
                            std::cerr << "dup2 error: " << errno << "\n";
                            exit(1);
                        }
                        close(pin[0]);
                        close(pin[1]);
                    });
            auto postsetup_pipe = [&](auto &&s, auto &&pipe, auto fd) {
                visit(
                    s,
                    [&](inherit) {
                        // nothing, inheritance is by default
                    },
                    [&](auto &) {
                        // while ((dup2(pipe[1], STDERR_FILENO) == -1) && (errno == EINTR)) {}
                        if (dup2(pipe[1], fd) == -1) {
                            std::cerr << "dup2 error: " << errno << "\n";
                            exit(1);
                        }
                        close(pipe[0]);
                        close(pipe[1]);
                    });
            };
            postsetup_pipe(out, pout, STDOUT_FILENO);
            postsetup_pipe(err, perr, STDERR_FILENO);
            // child
            if (execve(args2[0], args2.data(), environ) == -1) {
                std::cerr << "execve error: " << errno << "\n";
                exit(1);
            }
        }
        visit(
                in,
                [&](inherit) {
                    // nothing, we close child by default
                },
                [&](path &) {
                    // nothing
                },
                [&](string &s) {
                    close(pin[0]);
                    SW_UNIMPLEMENTED;
                    /*ex.register_write_handle(pin[1], [&s](auto &&buf, auto count) {
                        s.append(buf, count);
                    });*/
                },
                [&](stream_callback &cb) {
                    close(pin[0]);
                    SW_UNIMPLEMENTED;
                    /*ex.register_write_handle(pin[1], [&cb](auto &&buf, auto count) {
                        cb(string_view{buf, count});
                    });*/
                }
        );
        auto postsetup_pipe = [&](auto &&s, auto &&pipe) {
            visit(
                s,
                [&](inherit) {
                    // nothing, inheritance is by default
                },
                [&](path &) {
                    // nothing
                },
                [&](string &s) {
                    close(pipe[1]);
                    ex.register_read_handle(pipe[0], [&s](auto &&buf, auto count) {
                        s.append(buf, count);
                    });
                },
                [&](stream_callback &cb) {
                    close(pipe[1]);
                    ex.register_read_handle(pipe[0], [&cb](auto &&buf, auto count) {
                        cb(string_view{buf, count});
                    });
                }
            );
        };
        postsetup_pipe(out, pout);
        postsetup_pipe(err, perr);
        ex.register_process(pidfd, [&, pid, pidfd, pin = pin[1], pout = pout[0], perr = perr[0], cb]() {
            scope_exit se{[&] {
                visit(
                        in,
                        [&](inherit) {
                            // nothing, we close child by default
                        },
                        [&](path &) {
                            close(pin);
                        },
                        [&](auto &) {
                            close(pin);
                            SW_UNIMPLEMENTED;
                            //ex.unregister_write_handle(pipe);
                        }
                );
                auto postsetup_pipe = [&](auto &&s, auto &&pipe) {
                    visit(
                        s,
                        [&](inherit) {
                            // nothing, inheritance is by default
                        },
                        [&](path &) {
                            close(pipe);
                        },
                        [&](auto &) {
                            close(pipe);
                            ex.unregister_read_handle(pipe);
                        }
                    );
                };
                postsetup_pipe(out, pout);
                postsetup_pipe(err, perr);
                close(pidfd);
            }};

            int wstatus;
            if (waitpid(pid, &wstatus, 0) == -1) {
                throw std::runtime_error{"error waitpid: " + std::to_string(errno)};
            }
            if (WIFSIGNALED(wstatus)) {
                int exit_code = WTERMSIG(wstatus);
                cb(exit_code);
            } else if (WIFEXITED(wstatus)) {
                int exit_code = WEXITSTATUS(wstatus);
                cb(exit_code);
            }
        });
#endif
    }
    void run_macos(auto &&ex, auto &&cb) {
#if defined (__APPLE__)
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

        // use simple posix_spawn atm
        pid_t pid;
        auto r = posix_spawn(&pid, args2[0], 0, 0, args2.data(), environ);
        if (r) {
            throw std::runtime_error{"can't posix_spawn: "s + std::to_string(errno)};
        }

        ex.register_process(pid, [pid, cb](){
            int wstatus;
            if (waitpid(pid, &wstatus, 0) == -1) {
                throw std::runtime_error{"error waitpid: " + std::to_string(errno)};
            }
            if (WIFSIGNALED(wstatus)) {
                int exit_code = WTERMSIG(wstatus);
                cb(exit_code);
            } else if (WIFEXITED(wstatus)) {
                int exit_code = WEXITSTATUS(wstatus);
                cb(exit_code);
            }
        });
#endif
    }
    void run(auto &&ex, auto &&cb) {
        //log_trace(print());
        try {
#ifdef _WIN32
            run_win32(ex, cb);
#elif defined(__linux__)
            run_linux(ex, cb);
#elif defined(__APPLE__)
            run_macos(ex, cb);
#else
#error "unknown platform"
#endif
        } catch (std::exception &e) {
            throw std::runtime_error{"error during command start:\n"s + print() + "\n" + e.what()};
        }
    }
    void run(auto &&ex) {
        run(ex, [&]() {
            if (!ok()) {
                throw std::runtime_error{get_error_message()};
            }
        });
    }
    void run() {
        executor ex;
        run(ex);
        ex.run();
    }

    void finish() {
        d.close();
    }
    void terminate() {
        TerminateProcess(d.pi.hProcess, 1);
        finish();
    }

    void add(auto &&p) {
        if constexpr (requires {arguments.push_back(p);}) {
            arguments.push_back(p);
        } else {
            for (auto &&a : p) {
                add(a);
            }
        }
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

    void operator<(const path &p) {
        in = p;
    }
    void operator>(const path &p) {
        out = p;
    }
    void operator|(auto &c) {
        {
            command_pointer_holder ch;
            ch.r = &c;
            out = ch;
        }
        {
            command_pointer_holder ch;
            ch.r = this;
            c.in = ch;
        }
    }
    bool is_pipe_child() const {
        auto p = std::get_if<command_pointer_holder>(&in);
        return p;
    }
    bool is_pipe_leader() const {
        auto p = std::get_if<command_pointer_holder>(&out);
        return p && !is_pipe_child();
    }
    void pipe_iterate(auto &&in) {
        auto f = [&](auto f, auto c) -> void {
            if (auto p = std::get_if<command_pointer_holder>(&c->out)) {
                in(*p);
                f(f,p->r);
            }
        };
        if (auto p = std::get_if<command_pointer_holder>(&out)) {
            f(f,p->r);
        }
    }
    void terminate_chain() {
        if (is_pipe_leader()) {
            if (d.pi.hProcess) {
                //SW_UNIMPLEMENTED;
                terminate();
            }
        } else {
            auto ch = std::get_if<command_pointer_holder>(&in);
            ch->r->terminate_chain();
        }
    }
    void operator||(const raw_command &c) {
        SW_UNIMPLEMENTED;
    }
    void operator&&(const raw_command &c) {
        SW_UNIMPLEMENTED;
    }

    bool ok() const {
        return exit_code && *exit_code == 0;
    }
    string get_error_code() const {
        if (exit_code) {
            return format("process exit code: {}", *exit_code);
        }
        return "process did not start";
    }
    string get_error_message() {
        if (ok()) {
            return {};
        }
        string t;
        if (auto e = std::get_if<string>(&err); e && !e->empty()) {
            t = *e;
        } else if (auto e = std::get_if<string>(&out); e && !e->empty()) {
            t = *e;
        } else if (!out_text.empty()) {
            t = out_text;
        }
        /*if (!t.empty()) {
            t = "\nerror:\n" + t;
        }*/
        return format("{}\nerror:\n{}", get_error_code(), t);
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
        if (auto p = std::get_if<path>(&cmd.in)) {
            h ^= std::hash<path>()(*p);
        }
        if (auto p = std::get_if<path>(&cmd.out)) {
            h ^= std::hash<path>()(*p);
        }
        if (auto p = std::get_if<path>(&cmd.err)) {
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
                        std::transform(s.begin(), s.end(), s.begin(), tolower);
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

    void process_deps(){} // msvc 17.5P2 bug workaround

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
                    return format("new command: hash = {}, {}", (size_t)c.c->hash(), c.c->print());
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
            if (auto ch = std::get_if<command_pointer_holder>(&out)) {
                return ch->io->outdated(explain);
            }
            return true;
        }
        return false;
    }

    void operator<(const path &p) {
        base::operator<(p);
        inputs.insert(p);
    }
    void operator>(const path &p) {
        base::operator>(p);
        outputs.insert(p);
    }
    void operator|(auto &c) {
        {
            command_pointer_holder ch;
            ch.r = &c;
            ch.io = &c;
            out = ch;
        }
        {
            command_pointer_holder ch;
            ch.r = this;
            ch.io = this;
            c.in = ch;
        }
    }

    string name() const {
        if (!name_.empty()) {
            return name_;
        }
        if (!outputs.empty()) {
            string s = "generating: ";
            for (auto &&o : outputs) {
                s += format("\"{}\", ", (const char *)o.u8string().c_str());
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
        for (int i = 0; auto &&a : arguments) {
            if (i++) {
                s += "    ";
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
            s += "\n\n";
        }
        s += visit(t,[](auto &&v){return v.epilog;});
        write_file(fn, s);
        fs::permissions(fn, fs::status(fn).permissions() | fs::perms::owner_exec);
    }
    static shell_type detect_shell() {
#ifdef _WIN32
        return io_command::shell::cmd{};
#else
        return io_command::shell::sh{};
#endif
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

                    c.run();

                    auto &str = std::get<string>(c.out).empty() ? std::get<string>(c.err) : std::get<string>(c.out);
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

                    auto &s = std::get<string>(out);
                    auto pos = s.find('\n');
                    add_deps(s.data() + pos + 1);
                });
            }
        }
    }
};

struct gcc_command : io_command {
    path deps_file;

    void run(auto &&ex, auto &&cb) {
        err = ""s;
        out = ""s;

        io_command::run(ex, cb);
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

        mmap_file<char> f{deps_file};
        string_view sv{f.p, f.sz};
        auto p = sv.find(": ");
        if (p == -1) {
            SW_UNIMPLEMENTED;
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

struct command_executor {
    static void create_output_dirs(auto &&commands) {
        std::unordered_set<path> dirs;
        for (auto &&c : commands) {
            visit(*c, [&](auto &&c) {
                if (!c.working_directory.empty()) {
                    dirs.insert(c.working_directory);
                }
                for (auto &&o : c.outputs) {
                    if (auto p = o.parent_path(); !p.empty()) {
                        dirs.insert(p);
                    }
                }
            });
        }
        for (auto &&d : dirs) {
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

    struct pending_commands {
        std::deque<command *> commands;

        void push_back(command *cmd) {
            commands.push_back(cmd);
        }
        bool empty() const {
            return commands.empty() || std::ranges::all_of(commands, [](auto &&cmd) {
                       return visit(*cmd, [&](auto &&c) {
                           return c.simultaneous_jobs && c.simultaneous_jobs == 0;
                       });
                })
                ;
        }
        command *next() {
            auto it = std::ranges::find_if(commands, [](auto &&cmd) {
                return visit(*cmd, [&](auto &&c) {
                    return !c.simultaneous_jobs || c.simultaneous_jobs;
                });
            });
            auto c = *it;
            commands.erase(it);
            return c;
        }
    };

    pending_commands pending_commands_;
    int running_commands{0};
    size_t maximum_running_commands{std::thread::hardware_concurrency()};
    executor ex;
    executor *ex_external{nullptr};
    std::vector<command*> external_commands;
    // this number differs with external_commands.size() because we have:
    // 1. pipes a | b | ...
    // 2. command sequences a; b; c; ...
    // 3. OR or AND chains: a || b || ... ; a && b && ...
    int number_of_commands{};
    int command_id{};
    std::vector<command*> errors;
    int ignore_errors{0};
    bool explain_outdated{};

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
    bool is_stopped() const {
        return ignore_errors < errors.size();
    }
    void run_next_raw(auto &&cl, auto &&sln, auto &&cmd, auto &&c) {
        if (c.is_pipe_child()) {
            return;
        }
        ++command_id;
        auto run_dependents = [&]() {
            for (auto &&d : c.dependents) {
                visit(*(command *)d, [&](auto &&d1) {
                    if (!--d1.n_pending_dependencies) {
                        pending_commands_.push_back((command *)d);
                    }
                });
            }
        };
        c.processed = true;
        if (!c.outdated(explain_outdated)) {
            return run_dependents();
        }
        log_info("[{}/{}] {}", command_id, number_of_commands, c.name());
        log_trace(c.print());
        try {
            ++running_commands;
            if (c.simultaneous_jobs) {
                --(*c.simultaneous_jobs);
            }

            // use GetProcessTimes or similar for time
            // or get times directly from OS
            c.start = std::decay_t<decltype(c)>::clock::now();

            c.run(get_executor(), [&, run_dependents, cmd]() {
                if (c.simultaneous_jobs) {
                    ++(*c.simultaneous_jobs);
                }
                --running_commands;

                c.end = std::decay_t<decltype(c)>::clock::now();
                if (!c.ok()) {
                    errors.push_back(cmd);
                } else {
                    if constexpr (requires { c.process_deps(); }) {
                        c.process_deps();
                    }
                    if (c.cs) {
                        // pipe commands are not added yet - and they must be added when they are finished
                        c.cs->add(c);
                    }
                    run_dependents();
                }
                if (cl.save_executed_commands || cl.save_failed_commands && !c.ok()) {
                    c.save(get_saved_commands_dir(sln));
                }
                run_next(cl, sln);
            });
        } catch (std::exception &e) {
            c.out_text = e.what();
            if (c.is_pipe_child()) {
                c.terminate_chain();
                return;
            }
            if (c.is_pipe_leader()) {
                c.terminate_chain();
            }
            if (c.simultaneous_jobs) {
                ++(*c.simultaneous_jobs);
            }
            --running_commands;
            errors.push_back(cmd);
            if (cl.save_executed_commands || cl.save_failed_commands) {
                // c.save(get_saved_commands_dir(sln));//save not started commands?
            }
            run_next(cl, sln);
        }
    }
    void run_next(auto &&cl, auto &&sln) {
        while (running_commands < maximum_running_commands && !pending_commands_.empty() && !is_stopped()) {
            auto cmd = pending_commands_.next();
            visit(*cmd, [&](auto &&c) {
                run_next_raw(cl, sln, cmd, c);
            });
        }
    }
    void run(auto &&cl, auto &&sln) {
        prepare(cl, sln);

        // initial set of commands
        for (auto &&c : external_commands) {
            visit(*c, [&](auto &&c1) {
                if (c1.dependencies.empty()) {
                    pending_commands_.push_back(c);
                }
            });
        }
        run_next(cl, sln);
        get_executor().run();
    }
    void prepare(auto &&cl, auto &&sln) {
        prepare1(cl, sln);
        create_output_dirs(external_commands);
        make_dependencies(external_commands);
        check_dag(external_commands);
    }
    void prepare1(auto &&cl, auto &&sln) {
        visit_any(
            cl.c,
            [&](auto &b) requires requires {b.explain_outdated;} {
            explain_outdated = b.explain_outdated.value;
        });
        if (cl.jobs) {
            maximum_running_commands = cl.jobs;
        }
        for (auto &&c : external_commands) {
            visit(*c, [&](auto &&c) {
                if (cl.rebuild_all) {
                    c.always = true;
                }
                if (!c.is_pipe_child()) {
                    ++number_of_commands;
                }
                if (auto p = std::get_if<path>(&c.in)) {
                    c.inputs.insert(*p);
                }
                if (auto p = std::get_if<path>(&c.out)) {
                    c.outputs.insert(*p);
                }
                if (auto p = std::get_if<path>(&c.err)) {
                    c.outputs.insert(*p);
                }
            });
        }
        for (auto &&c : external_commands) {
            visit(*c, [&](auto &&c) {
                if (c.is_pipe_leader()) {
                    c.pipe_iterate([&](auto &&ch) {
                        c.inputs.insert(ch.io->inputs.begin(), ch.io->inputs.end());
                        //c.outputs.insert(ch.io->outputs.begin(), ch.io->outputs.end());
                    });
                }
            });
        }
    }
    path get_saved_commands_dir(auto &&sln) {
        return sln.work_dir / "rsp";
    }

    void operator+=(std::vector<command> &commands) {
        for (auto &&c : commands) {
            external_commands.push_back(&c);
        }
    }
};

} // namespace sw
