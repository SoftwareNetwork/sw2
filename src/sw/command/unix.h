// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2022 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include "helpers/common.h"

#include "sys/linux.h"
#include "sys/macos.h"
#include "sys/mmap.h"

#if defined(__linux) || defined(__APPLE__)

namespace sw {

template <bool Output>
struct command_stream {
    static constexpr auto Input = !Output;

    using second_end_of_pipe = command_stream<!Output>*;
    using stream_callback = std::function<void(string_view)>;
    struct inherit {};
    struct close_ {};
    // default mode is inheritance
    using stream = variant<inherit, close_, string, stream_callback, path, second_end_of_pipe>;

    stream s;
    struct pipe_type {
        int p[2]{-1,-1};
        int &r{p[0]};
        int &w{p[1]};
    };
    //int fd{-1};
    pipe_type pipe;

    command_stream() = default;
    command_stream(command_stream &&rhs) {
        s = rhs.s;
        this->pipe.p[0] = rhs.pipe.p[0];
        this->pipe.p[1] = rhs.pipe.p[1];
    }
    command_stream &operator=(command_stream &&rhs) {
        this->s = rhs.s;
        this->pipe.p[0] = rhs.pipe.p[0];
        this->pipe.p[1] = rhs.pipe.p[1];
        return *this;
    }
    command_stream &operator=(const command_stream &rhs) {
        this->s = rhs.s;
        this->pipe.p[0] = rhs.pipe.p[0];
        this->pipe.p[1] = rhs.pipe.p[1];
        return *this;
    }
    command_stream &operator=(const stream &s) {
        this->s = s;
        return *this;
    }
    /*operator stream &() {
        return s;
    }
    operator stream *() {
        return &s;
    }*/

    auto pre_create_command(auto os_handle, auto &&ex) {
        auto mkpipe = [&]() {
            if (::pipe(pipe.p) == -1) {
                fprintf(stderr, "Error creating pipe\n");
            }
        };

        visit(
            s,
            [&](inherit) {
                // nothing, inheritance is by default?
            },
            [&](close_) {
                // empty
            },
            [&](path &fn) {
                if constexpr (Input) {
                    pipe.r = open(fn.string().c_str(), O_RDONLY);
                    if (pipe.r == -1) {
                        throw std::runtime_error(std::format("cannot open file for reading: {}", fn.string()));
                    }
                } else {
                    pipe.w = open(fn.string().c_str(), O_CREAT | O_WRONLY | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP);
                    if (pipe.w == -1) {
                        throw std::runtime_error(std::format("cannot open file for writing: {}", fn.string()));
                    }
                }
            },
            [&](second_end_of_pipe &e) {
                SW_UNIMPLEMENTED;
                // ok, but verify
                if constexpr (Input) {
                } else {
                }
            },
            [&](auto &) {
                mkpipe();
            });
    }
    void post_create_command(auto &&ex) {
        visit(
            s,
            [&](inherit) {
                // empty
            },
            [&](close_) {
                // empty
            },
            [&](string &s) {
                if constexpr (Input) {
                    close(pipe.r);
                    pipe.r = -1;
                    SW_UNIMPLEMENTED;
                    //ex.register_write_handle(pin[1], [&s](auto &&buf, auto count) {
                    //s.append(buf, count);
                    //});
                } else {
                    close(pipe.w);
                    pipe.w = -1;
                    ex.register_read_handle(pipe.r, [&s](auto &&buf, auto count) {
                        s.append(buf, count);
                    });
                }
            },
            [&](second_end_of_pipe &) {
                SW_UNIMPLEMENTED;
            },
            [&](stream_callback &cb) {
                if constexpr (Input) {
                    close(pipe.r);
                    pipe.r = -1;
                    SW_UNIMPLEMENTED;
                    //ex.register_write_handle(pin[1], [&cb](auto &&buf, auto count) {
                    //cb(string_view{buf, count});
                    //});
                } else {
                    close(pipe.w);
                    pipe.w = -1;
                    ex.register_read_handle(pipe.r, [&cb](auto &&buf, auto count) {
                        cb(string_view{buf, count});
                    });
                }
            },
            [&](path &fn) {
                if constexpr (Input) {
                    close(pipe.w);
                } else {
                    close(pipe.r);
                }
            });
    }
    void inside_fork(int fd) {
        visit(
                s,
                [&](inherit) {
                    // empty
                },
                [&](close_) {
                    close(fd);
                },
            [&](auto &) {
                    if constexpr (Input) {
                        // while ((dup2(pipe[1], STDERR_FILENO) == -1) && (errno == EINTR)) {}
                        if (dup2(pipe.r, fd) == -1) {
                            std::cerr << "dup2 r error: " << errno << "\n";
                            exit(1);
                        }
                    } else {
                        // while ((dup2(pipe[1], STDERR_FILENO) == -1) && (errno == EINTR)) {}
                        if (dup2(pipe.w, fd) == -1) {
                            std::cerr << "dup2 w error: " << errno << "\n";
                            exit(1);
                        }
                    }
                    close(pipe.r);
                    close(pipe.w);
                });
    }
    void post_exit_command(auto &&ex) {
        visit(
                s,
                [&](inherit) {
                    // nothing, we close child by default
                },
                [&](close_) {
                    // nothing
                },
                [&](path &) {
                    // nothing
                },
                [&](auto &) {
                    if constexpr (Input) {
                        close(pipe.w);
                        SW_UNIMPLEMENTED;
                        //ex.unregister_write_handle(pipe);
                    } else {
                        close(pipe.r);
                        ex.unregister_read_handle(pipe.r);
                    }
                }
        );
    }
    void finish() {
        // we close in post_exit()
        //close(pipe.r);
        //close(pipe.w);
    }

    template <typename T>
    T &get() {
        return std::get<T>(s);
    }
    template <typename T>
    operator T &() {
        return std::get<std::decay_t<T>>(s);
    }
};

struct raw_command {
    using argument = variant<string,string_view,path>;
    std::vector<argument> arguments;
    path working_directory;
    std::map<string, string> environment;

    //bool started{}; // state?
    std::optional<int> exit_code;

    command_stream<false> in;
    command_stream<true> out, err;
    string out_text; // filtered, workaround
    pid_t pid{-1};
    //
    bool detach{};
    bool exec{};
    std::chrono::seconds time_limit{};

    // sync()
    // async()
    /*bool started() const {
        return pid != -1;
    }*/
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
    }
#if defined(__linux__)
    void run_platform(auto &&ex, auto &&cb) {
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

        in.pre_create_command(STDIN_FILENO, ex);
        out.pre_create_command(STDOUT_FILENO, ex);
        err.pre_create_command(STDERR_FILENO, ex);

        clone_args cargs{};
        cargs.flags |= CLONE_PIDFD;
        cargs.flags |= CLONE_VFORK; // ?
        //cargs.exit_signal = SIGCHLD;
        int pidfd;
        cargs.pidfd = &pidfd;
        pid = exec ? 0 : clone3(&cargs, sizeof(cargs));
        if (pid == -1) {
            throw std::runtime_error{"can't clone3: "s + std::to_string(errno)};
        }
        if (pid == 0) {
            in.inside_fork(STDIN_FILENO);
            out.inside_fork(STDOUT_FILENO);
            err.inside_fork(STDERR_FILENO);

            if (time_limit.count()) {
                struct rlimit old, newl{};
                newl.rlim_cur = time_limit.count();
                newl.rlim_max = time_limit.count();
                if (prlimit(0, RLIMIT_CPU, &newl, &old) == -1) {
                    std::cerr << "prlimit error: " << errno << "\n";
                    exit(1);
                }
            }

            // child
            if (execve(args2[0], args2.data(), environ) == -1) {
                std::cerr << "execve error: " << errno << "\n";
                exit(1);
            }
        }
        in.post_create_command(ex);
        out.post_create_command(ex);
        err.post_create_command(ex);
        ex.register_process(pidfd, [&, pidfd, cb]() {
            scope_exit se{[&] {
                in.post_exit_command(ex);
                out.post_exit_command(ex);
                err.post_exit_command(ex);
                close(pidfd);
            }};

            int wstatus;
            if (waitpid(pid, &wstatus, 0) == -1) {
                throw std::runtime_error{"error waitpid: " + std::to_string(errno)};
            }
            if (WIFSIGNALED(wstatus)) {
                this->exit_code = WTERMSIG(wstatus);
                cb();
            } else if (WIFEXITED(wstatus)) {
                this->exit_code = WEXITSTATUS(wstatus);
                cb();
            }
        });
    }
#endif
#if defined (__APPLE__)
    void run_platform(auto &&ex, auto &&cb) {
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

        in.pre_create_command(STDIN_FILENO, ex);
        out.pre_create_command(STDOUT_FILENO, ex);
        err.pre_create_command(STDERR_FILENO, ex);

        // use simple posix_spawn atm
        /*auto r = posix_spawn(&pid, args2[0], 0, 0, args2.data(), environ);
        if (r) {
            throw std::runtime_error{"can't posix_spawn: "s + std::to_string(errno)};
        }*/

        // do we have a race here?
        pid = exec ? 0 : fork();
        if (pid == -1) {
            throw std::runtime_error{"can't clone3: "s + std::to_string(errno)};
        }
        if (pid == 0) {
            in.inside_fork(STDIN_FILENO);
            out.inside_fork(STDOUT_FILENO);
            err.inside_fork(STDERR_FILENO);

            if (time_limit.count()) {
                struct rlimit old, newl{};
                newl.rlim_cur = time_limit.count();
                newl.rlim_max = time_limit.count();
                if (setrlimit(RLIMIT_CPU, &newl) == -1) {
                    std::cerr << "setrlimit error: " << errno << "\n";
                    exit(1);
                }
            }

            // child
            if (execve(args2[0], args2.data(), environ) == -1) {
                std::cerr << "execve error: " << errno << "\n";
                exit(1);
            }
        }
        in.post_create_command(ex);
        out.post_create_command(ex);
        err.post_create_command(ex);
        ex.register_process(pid, [this, &ex, cb]() {
            scope_exit se{[&] {
                in.post_exit_command(ex);
                out.post_exit_command(ex);
                err.post_exit_command(ex);
                //close(pidfd);
            }};

            int wstatus;
            if (waitpid(pid, &wstatus, 0) == -1) {
                throw std::runtime_error{"error waitpid: " + std::to_string(errno)};
            }
            if (WIFSIGNALED(wstatus)) {
                exit_code = WTERMSIG(wstatus);
                cb();
            } else if (WIFEXITED(wstatus)) {
                exit_code = WEXITSTATUS(wstatus);
                cb();
            }
        });
    }
#endif
    void run(auto &&ex, auto &&cb) {
        //log_trace(print());
        try {
            run_platform(ex, cb);
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
    auto run() {
        executor ex;
        run(ex);
        ex.run();
        return *exit_code;
    }

    void finish() {
        in.finish();
        out.finish();
        err.finish();
    }
    void terminate() {
        kill(pid, SIGKILL);
        finish();
    }

    void add(auto &&p) {
        if constexpr (requires {std::to_string(p);}) {
            add(std::to_string(p));
        } else if constexpr (requires {arguments.push_back(p);}) {
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
        SW_UNIMPLEMENTED;
        /*{
            command_pointer_holder ch;
            ch.r = &c;
            out = ch;
        }
        {
            command_pointer_holder ch;
            ch.r = this;
            c.in = ch;
        }*/
    }
    bool is_pipe_child() const {
        return false;
        SW_UNIMPLEMENTED;
        //auto p = std::get_if<command_pointer_holder>(&in.s);
        //return p;
    }
    bool is_pipe_leader() const {
        return false;
        SW_UNIMPLEMENTED;
        //auto p = std::get_if<command_pointer_holder>(&out.s);
        //return p && !is_pipe_child();
    }
    void pipe_iterate(auto &&in) {
        SW_UNIMPLEMENTED;
        /*auto f = [&](auto f, auto c) -> void {
            if (auto p = std::get_if<command_pointer_holder>(&c->out.s)) {
                in(*p);
                f(f,p->r);
            }
        };
        if (auto p = std::get_if<command_pointer_holder>(&out.s)) {
            f(f,p->r);
        }*/
    }
    void terminate_chain() {
        if (is_pipe_leader()) {
        } else {
            SW_UNIMPLEMENTED;
            //auto ch = std::get_if<command_pointer_holder>(&in.s);
            //ch->r->terminate_chain();
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
            return std::format("process exit code: {}", *exit_code);
        }
        return "process did not start";
    }
    string get_error_message() {
        if (ok()) {
            return {};
        }
        string t;
        if (0) {
        } else if (auto e = std::get_if<string>(&err.s); e && !e->empty()) {
            t = *e;
        } else if (auto e = std::get_if<path>(&err.s); e && !e->empty()) {
            t = read_file(*e);
        } else if (auto e = std::get_if<string>(&out.s); e && !e->empty()) {
            t = *e;
        } else if (auto e = std::get_if<path>(&out.s); e && !e->empty()) {
            t = read_file(*e);
        } else if (!out_text.empty()) {
            t = out_text;
        }
        /*if (!t.empty()) {
            t = "\nerror:\n" + t;
        }*/
        return std::format("{}\nerror:\n{}", get_error_code(), t);
    }
};

} // namespace sw

#endif
