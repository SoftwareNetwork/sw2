// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2022 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include "../helpers/common.h"
#include "../sys/win32.h"

#if defined(_WIN32)

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
    HANDLE h = 0;
    win32::pipe pipe;

    command_stream() = default;
    command_stream(command_stream &&rhs) {
        s = rhs.s;
        h = rhs.h;
    }
    command_stream &operator=(command_stream &&rhs) {
        this->s = rhs.s;
        this->h = rhs.h;
        return *this;
    }
    command_stream &operator=(const command_stream &rhs) {
        this->s = rhs.s;
        this->h = rhs.h;
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
        visit(
            s,
            [&](inherit) {
                h = GetStdHandle(os_handle);
            },
            [&](close_) {
                // empty
            },
            [&](path &fn) {
                SECURITY_ATTRIBUTES sa = {0};
                sa.bInheritHandle = TRUE;
                h = WINAPI_CALL_HANDLE(
                    CreateFileW(fn.wstring().c_str(),
                        Output ? GENERIC_WRITE : GENERIC_READ, 0, &sa,
                        Output ? CREATE_ALWAYS : OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0));
            },
            [&](second_end_of_pipe &e) {
                SW_UNIMPLEMENTED;
                // ok, but verify
                if constexpr (Input) {
                    h = e->pipe.r;
                } else {
                    pipe.init_double(true);
                    h = pipe.w;
                }
            },
            [&](auto &) {
                pipe.init(true);
                if constexpr (Input) {
                    ex.register_handle(pipe.w);
                    h = pipe.r;
                } else {
                    ex.register_handle(pipe.r);
                    h = pipe.w;
                }
            });
        return h;
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
                    SW_UNIMPLEMENTED;
                    /*pipe.r.reset();
                    ex.write_async(pipe.w, [&](auto &&f, auto &&buf, auto &&ec) mutable {
                        if (!ec) {
                            s += buf;
                            ex.write_async(pipe.w, std::move(f));
                        }
                    });*/
                } else {
                    pipe.w.reset();
                    ex.read_async(pipe.r, [&](auto &&f, auto &&buf, auto &&ec) mutable {
                        if (!ec) {
                            s += buf;
                            ex.read_async(pipe.r, std::move(f));
                        }
                    });
                }
            },
            [&](second_end_of_pipe &) {
                SW_UNIMPLEMENTED;
                if constexpr (Input) {
                    //CloseHandle(d.si.StartupInfo.hStdInput);
                } else {
                   //CloseHandle(h);
                }
            },
            [&](stream_callback &cb) {
                if constexpr (Input) {
                    SW_UNIMPLEMENTED;
                    /*pipe.r.reset();
                    ex.write_async(pipe.w, [&, cb, s = string{}](auto &&f, auto &&buf, auto &&ec) mutable {
                        if (!ec) {
                            s += buf;
                            if (auto p = s.find_first_of("\r\n"); p != -1) {
                                cb(string_view(s.data(), p));
                                p = s.find("\n", p);
                                s = s.substr(p + 1);
                            }
                            ex.write_async(pipe.w, std::move(f));
                        }
                    });*/
                } else {
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
                }
            },
            [&](path &fn) {
                CloseHandle(h);
            });
    }
    void inside_fork(int fd) {
        visit(
                s,
                [&](inherit) {
                    // empty
                },
                [&](close_) {
                },
            [&](auto &) {
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
                }
        );
    }
    void finish() {
        pipe = decltype(pipe){};
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

    struct process_data {
        STARTUPINFOEXW si = {0};
        PROCESS_INFORMATION pi = {0};
        win32::handle thread, process;

        process_data() {
            si.StartupInfo.cb = sizeof(si);
        }
        void close() {
            *this = process_data{};
        }
    };
    process_data d;
    //
    bool detach{};
    bool exec{};
    std::chrono::seconds time_limit{};

    // sync()
    // async()
    /*bool started() const {
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
        //ShellExecuteA(NULL, "open", "C:\\WINDOWS\\system32\\calc.exe", "/k ipconfig", 0, SW_SHOWNORMAL);
        //run_win32{}
    }
    void run_platform(auto &&ex, auto &&cb) {
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
        if (detach) {
            d.si.StartupInfo.dwFlags = DETACHED_PROCESS;
        }

        // pre command
        d.si.StartupInfo.hStdInput = in.pre_create_command(STD_INPUT_HANDLE, ex);
        d.si.StartupInfo.hStdOutput = out.pre_create_command(STD_OUTPUT_HANDLE, ex);
        d.si.StartupInfo.hStdError = err.pre_create_command(STD_ERROR_HANDLE, ex);

        auto mingw = is_mingw_shell();
        auto add_handle = [&](auto &&h) {
            if (!h) {
                return;
            }
                // mingw uses same stdout and stderr, so if we pass two same handles they are give an error 87
            //if (mingw) {
            // we always check for same handles because it can happen not only on mingw
                if (std::ranges::find(handles, h) != std::end(handles)) {
                    return;
                }
            //}
            handles.push_back(h);
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

        if (exec) {
            _exit(0);
        }
        if (detach) {
            finish();
            exit_code = 0;
            cb();
        }

        in.post_create_command(ex);
        out.post_create_command(ex);
        err.post_create_command(ex);

        win32::handle process_job;
        if (time_limit.count() != 0) {
            process_job = win32::create_job_object();

            JOBOBJECT_BASIC_LIMIT_INFORMATION li{};
            // number of 100ns
            li.PerProcessUserTimeLimit.QuadPart = time_limit.count() * 10 * 1'000'000;
            li.LimitFlags |= JOB_OBJECT_LIMIT_PROCESS_TIME;
            WINAPI_CALL(SetInformationJobObject(process_job, JobObjectBasicLimitInformation, &li, sizeof(li)));

            ex.register_job(process_job);
            WINAPI_CALL(AssignProcessToJobObject(process_job, d.process));
        }

        // If we do not create default job for main process, we have a race here.
        // If main process is killed before register_process() call, created process
        // won't stop.
        ex.register_process(d.pi.hProcess, d.pi.dwProcessId, [this, cb, process_job = std::move(process_job)](bool time_limit_hit) {
            if (time_limit_hit) {
                TerminateProcess(d.pi.hProcess, 1);
            }
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
            if (time_limit_hit) {
                if (auto p = std::get_if<path>(&err.s)) {
                    write_file(*p, std::format("time limit hit: {}", time_limit));
                } else {
                    err = std::format("time limit hit: {}", time_limit);
                }
            }
            cb();
        });
        /*visit(
            out.s,
            [&](command_pointer_holder &ch) {
                ch.r->run(ex, cb);
            },
            [&](auto &) {
            });*/
    }
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
        d.close();
    }
    void terminate() {
        TerminateProcess(d.pi.hProcess, 1);
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
            if (d.pi.hProcess) {
                //SW_UNIMPLEMENTED;
                terminate();
            }
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
