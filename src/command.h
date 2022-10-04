#pragma once

#include "helpers.h"
#include "win32.h"

#ifdef _WIN32
#include <windows.h>
#endif

namespace sw {

struct raw_command {
    using argument = variant<string,string_view,path>;
    std::vector<argument> arguments;
    path working_directory;
    std::map<string, string> environment;

    // exit_code
    // pid, pidfd

    // stdin,stdout,stderr
    using stream_callback = std::function<void(const string &, const std::error_code &)>;
    using stream = variant<std::monostate, string, stream_callback>;
    //stream in;
    stream out,err;
    win32::pipe pout, perr;

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
                                  quote(path{s});
                              },
                              [&](string_view &s) {
                                  quote(path{string{s.data(), s.size()}});
                              },
                              [&](path &p) {
                                  quote(p);
                              }});
        }
        return s;
    }

    // execute?
    void start_job_object() {
        //ShellExecuteA(NULL, "open", "C:\\WINDOWS\\system32\\calc.exe", "/k ipconfig", 0, SW_SHOWNORMAL);
        //run_win32{}
    }
    void run_win32() {
#ifdef _WIN32
        DWORD flags = 0;
        STARTUPINFOEXW si = { 0 };
        si.StartupInfo.cb = sizeof(si);
        PROCESS_INFORMATION pi = { 0 };
        LPVOID env = 0;
        LPCWSTR dir = 0;
        int inherit_handles = 1; // must be 1

        flags |= NORMAL_PRIORITY_CLASS;
        flags |= CREATE_UNICODE_ENVIRONMENT;
        flags |= EXTENDED_STARTUPINFO_PRESENT;
        // flags |= CREATE_NO_WINDOW;

        si.StartupInfo.dwFlags = STARTF_USESTDHANDLES;

        win32::executor e;
        win32::pipe out{true}, err{true};

        si.StartupInfo.hStdOutput = out.w;
        si.StartupInfo.hStdError = err.w;

        auto job = CreateJobObject(0, 0);
        if (!job) {
            throw std::runtime_error{"cannot CreateJobObject"};
        }
        if (!AssignProcessToJobObject(job, GetCurrentProcess())) {
            throw std::runtime_error{"cannot AssignProcessToJobObject"};
        }

        JOBOBJECT_EXTENDED_LIMIT_INFORMATION ji;
        if (!QueryInformationJobObject(job, JobObjectExtendedLimitInformation, &ji, sizeof(ji), 0)) {
            throw std::runtime_error{"cannot QueryInformationJobObject"};
        }
        ji.BasicLimitInformation.LimitFlags |= JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        if (!SetInformationJobObject(job, JobObjectExtendedLimitInformation, &ji, sizeof(ji))) {
            throw std::runtime_error{"cannot SetInformationJobObject"};
        }

        JOBOBJECT_ASSOCIATE_COMPLETION_PORT jcp{};
        jcp.CompletionPort = e.port;
        jcp.CompletionKey = (void *)10;
        if (!SetInformationJobObject(job, JobObjectAssociateCompletionPortInformation, &jcp, sizeof(jcp))) {
            throw std::runtime_error{"cannot SetInformationJobObject"};
        }

        std::wcout << printw() << "\n";
        auto r = CreateProcessW(0, printw().data(), 0, 0,
            inherit_handles,
            flags,
            env,
            dir,
            (LPSTARTUPINFOW)&si, &pi
        );
        if (!r) {
            auto err = GetLastError();
            throw std::runtime_error("CreateProcessW failed, code = " + std::to_string(err));
        }
        if (!AssignProcessToJobObject(job, pi.hProcess)) {
            throw std::runtime_error{"cannot AssignProcessToJobObject"};
        }
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        out.w.reset();
        err.w.reset();

        /*out.r.read_async(ex, [&](this auto &&f, auto &&buf, auto &&ec) {
            if (ec) {
                out.r.reset();
                return;
            }
            //std::cout << string{buf.data(), buf.data()+buf.size()};
            out.read_async(f);
        });
        err.r.read_async(ex, [&](this auto &&f, auto &&buf, auto &&ec) {
            if (ec) {
                err.r.reset();
                return;
            }
            err.read_async(f);
        });*/
        e.run();
#endif
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
                    //pipe.read_async(cb);
                }
            );
        };
        set_stream2(out, pout);
        set_stream2(err, perr);

        ex.register_process(pi.hProcess, pi.dwProcessId, [this, cb, h = pi.hProcess]() {
            DWORD exit_code;
            GetExitCodeProcess(h, &exit_code);
            CloseHandle(h);
            cb(exit_code);
        });
#endif
    }
    void run_linux() {
        // clone3()
    }
    void run_macos() {
        //
    }
    void run() {
#ifdef _WIN32
        run_win32();
#endif
    }
    void run(auto &&ex, auto &&cb) {
#ifdef _WIN32
        run_win32(ex, cb);
#endif
    }
    void run(auto &&ex) {
#ifdef _WIN32
        run(ex, [](auto){});
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

struct io_command : raw_command {
    std::set<path> inputs;
    std::set<path> outputs;
    std::set<path> implicit_inputs;
};

struct cl_exe_command : io_command {
    void run(auto &&ex) {
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

        out = err = ""s;
        add("/showIncludes");
        scope_exit se{[&]{arguments.pop_back();}};
        io_command::run(ex, [&](auto exit_code) {
            if (exit_code) {
                throw std::runtime_error(std::format("process exit code: {}", exit_code));
            }
            auto &str = std::get<string>(out);
            size_t p = 0;
            while ((p = str.find(msvc_prefix, p)) != -1) {
                p += msvc_prefix.size();
                implicit_inputs.insert(str.substr(p, str.find_first_of("\r\n", p) - p));
            }
        });
    }
};

using command = variant<io_command, cl_exe_command>;

struct command_executor {
    void run(auto &&commands) {
        win32::executor ex;
        auto job = win32::create_job_object();
        ex.register_job(job);

        for (auto &&c : commands) {
            visit(c, [&](auto &&c) {
                c.run(ex);
                ex.run();
            });
        }
    }
};

}
