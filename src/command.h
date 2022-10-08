// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2022 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include "helpers.h"
#include "win32.h"
#include "linux.h"
#include "macos.h"
#include "mmap.h"

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
    using stream = variant<std::monostate, string, stream_callback>;
    //stream in;
    stream out,err;
#ifdef _WIN32
    win32::pipe pout, perr;
#endif

    // sync()
    // async()

    std::wstring printw() {
        std::wstring s;
        for (auto &&a : arguments) {
            auto quote = [&](const std::wstring &as) {
                if (as.contains(L" ")) {
                    s += L"\"" + as + L"\" ";
                } else {
                    s += as + L" ";
                }
            };
            visit(a, overload{[&](string &s) {
                                  quote(path{s}.wstring());
                              },
                              [&](string_view &s) {
                                  quote(path{string{s.data(), s.size()}}.wstring());
                              },
                              [&](path &p) {
                                  quote(p.wstring());
                              }});
        }
        return s;
    }
    std::string print() {
        std::string s;
        for (auto &&a : arguments) {
            auto quote = [&](const std::string &as) {
                if (as.contains(" ")) {
                    s += "\"" + as + "\" ";
                } else {
                    s += as + " ";
                }
            };
            visit(a, overload{[&](string &s) {
                quote(s);
            },
                              [&](string_view &s) {
                                  quote(string{s.data(), s.size()});
                              },
                              [&](path &p) {
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
        std::wcout << printw() << "\n";

        DWORD flags = 0;
        STARTUPINFOEXW si = {0};
        si.StartupInfo.cb = sizeof(si);
        PROCESS_INFORMATION pi = {0};
        LPVOID env = 0;
        LPCWSTR dir = 0;
        int inherit_handles = 1; // must be 1

        flags |= NORMAL_PRIORITY_CLASS;
        flags |= CREATE_UNICODE_ENVIRONMENT;
        flags |= EXTENDED_STARTUPINFO_PRESENT;
        // flags |= CREATE_NO_WINDOW;

        si.StartupInfo.dwFlags = STARTF_USESTDHANDLES;

        auto set_stream = [&](auto &&s, auto &&h, auto &&stdh, auto &&pipe) {
            pipe.init(true);
            ex.register_handle(pipe.r);
            visit(
                s,
                [&](std::monostate) {
                    h = GetStdHandle(stdh);
                },
                [&](auto &) {
                    h = pipe.w;
                });
        };
        set_stream(out, si.StartupInfo.hStdOutput, STD_OUTPUT_HANDLE, pout);
        set_stream(err, si.StartupInfo.hStdError, STD_ERROR_HANDLE, perr);

        auto r = CreateProcessW(0, printw().data(), 0, 0, inherit_handles, flags, env, dir, (LPSTARTUPINFOW)&si, &pi);
        if (!r) {
            auto err = GetLastError();
            throw std::runtime_error("CreateProcessW failed, code = " + std::to_string(err));
        }
        CloseHandle(pi.hThread);
        pout.w.reset();
        perr.w.reset();

        auto set_stream2 = [&](auto &&s, auto &&pipe) {
            visit(
                s,
                [&](std::monostate) {
                },
                [&](string &s) {
                    ex.read_async(pipe.r, [&](this auto &&f, auto &&buf, auto &&ec) {
                        if (!ec) {
                            s += buf;
                            ex.read_async(pipe.r, f);
                        }
                    });
                },
                [&](stream_callback &cb) {
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
                }
            );
        };
        set_stream2(out, pout);
        set_stream2(err, perr);

        ex.register_process(pi.hProcess, pi.dwProcessId, [this, cb, h = pi.hProcess]() {
            DWORD exit_code;
            GetExitCodeProcess(h, &exit_code);
            cb(exit_code);
            CloseHandle(h); // we may want to use h in the callback
        });
#endif
    }
    void run_linux(auto &&ex, auto &&cb) {
#if defined(__linux__)
        std::wcout << printw() << "\n";

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
#ifdef _WIN32
        run(ex, [](auto){});
#elif defined(__linux__)
        run_linux(ex, [](auto){});
#endif
    }

    void add(auto &&p) {
        arguments.push_back(p);
    }
    void add(const char *p) {
        arguments.push_back(string{p});
    }
    auto operator+=(auto &&arg) {
        add(arg);
        return appender{[&](auto &&v){operator+=(v);}};
    }
};

struct command_hash {
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
};

struct io_command : raw_command {
    using clock = std::chrono::system_clock;
    using time_point = clock::time_point;
    using hash_type = command_hash;

    bool always{false};
    std::set<path> inputs;
    std::set<path> outputs;
    std::set<path> implicit_inputs;
    mutable hash_type h;
    time_point start, end;

    bool outdated(auto &&cs) const {
        return always || cs.outdated(*this);
    }
    void run(auto &&ex, auto &&cs) {
        if (!outdated(cs)) {
            return;
        }
        // use GetProcessTimes or similar for time
        start = clock::now();
        raw_command::run(ex, [&](auto exit_code) {
            end = clock::now();
            if (exit_code) {
                throw std::runtime_error(
                    //format("process exit code: {}\nerror: {}", exit_code, std::get<string>(err))
                    "process exit code: " + std::to_string(exit_code) + "\nerror: " + std::get<string>(err) + ""
                );
            }
            cs.add(*this);
        });
    }
    auto hash() const {
        if (!h) {
            h(*this);
        }
        return h;
    }
};

struct cl_exe_command : io_command {
    void run(auto &&ex, auto &&cs) {
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

        err = ""s;

        // < 14.27  1927 (version 16.7)
        /*add("/showIncludes");
        scope_exit se{[&]{arguments.pop_back();}};
        out = [&](auto sv) {
            size_t p = 0;
            while ((p = sv.find(msvc_prefix, p)) != -1) {
                p += msvc_prefix.size();
                p = sv.find_first_not_of(' ', p);
                auto fn = sv.substr(p, sv.find_first_of("\r\n", p) - p);
                implicit_inputs.insert(string{fn.data(), fn.data() + fn.size()});
            }
        };*/

        // >= 14.27  1927 (version 16.7)
        // https://learn.microsoft.com/en-us/cpp/build/reference/sourcedependencies?view=msvc-170
        add("/sourceDependencies-");
        scope_exit se{[&]{arguments.pop_back();}};
        out = ""s;

        io_command::run(ex, cs);
    }
};

struct gcc_command : io_command {
    void run(auto &&ex, auto &&cs) {
        err = ""s;
        out = ""s;

        io_command::run(ex, cs);
    }
};

using command = variant<io_command, cl_exe_command, gcc_command>;

struct command_storage {
    struct command_data {
        fs::file_time_type mtime;
        io_command::hash_type hash;
    };

    using mmap_type = mmap_file<>;

    mmap_type f_commands, f_files;
    std::unordered_set<path> files;
    mmap_type::stream cmd_stream;
    std::unordered_map<uint64_t, command_data> commands;

    command_storage() : command_storage{"commands.bin"} {}
    command_storage(const path &fn)
            : f_commands{fn, mmap_type::rw{}}
            , f_files{path{fn} += ".files", mmap_type::rw{}}
            , cmd_stream{f_commands.get_stream()} {
        if (cmd_stream.size() == 0) {
            return;
        }
        while (auto s = cmd_stream.read_record()) {
        }
    }

    bool outdated(auto &&cmd) const {
        return true;
    }
    void add(auto &&cmd) {
        uint64_t n{0};
        auto ins = [&](auto &&v) {
            files.insert(v.begin(), v.end());
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

struct command_executor {
    void run(auto &&tgt) {
        command_storage cs;
        executor ex;
#ifdef _WIN32
        auto job = win32::create_job_object();
        ex.register_job(job);
#endif

        for (auto &&c : tgt.commands) {
            visit(c, [&](auto &&c) {
                c.run(ex, cs);
                ex.run();
            });
        }
    }
};

}
