#pragma once

#include "helpers.h"
#include "win32.h"
#include "mmap.h"

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
    using stream_callback = std::function<void(string_view)>;
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
    bool always{false};
    std::set<path> inputs;
    std::set<path> outputs;
    std::set<path> implicit_inputs;

    bool outdated() const {
        return always || true;
    }

    void run(auto &&ex, auto &&cb) {
        if (!outdated()) {
            return;
        }
        raw_command::run(ex, [&, cb](auto exit_code) {
            if (exit_code) {
                throw std::runtime_error(
                    std::format("process exit code: {}\nerror: {}", exit_code, std::get<string>(err)));
            }
            cb();
        });
    }
};

struct cl_exe_command : io_command {
    void run(auto &&ex, auto &&cb) {
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
        out = [&](auto sv) {
            size_t p = 0;
            while ((p = sv.find(msvc_prefix, p)) != -1) {
                p += msvc_prefix.size();
                auto fn = sv.substr(p, sv.find_first_of("\r\n", p) - p);
                implicit_inputs.insert(string{fn.data(),fn.data()+fn.size()});
            }
        };

        add("/showIncludes");
        scope_exit se{[&]{arguments.pop_back();}};

        io_command::run(ex, cb);
    }
};

using command = variant<io_command, cl_exe_command>;

struct command_storage {
    using mmap_type = mmap_file<>;

    mmap_type commands, files;

    command_storage() : command_storage{"commands.bin"} {}
    command_storage(const path &fn) : commands{fn, mmap_type::rw{}}, files{path{fn} += ".files", mmap_type::rw{}} {}


};

struct command_executor {
    void run(auto &&tgt) {
        command_storage cs;
        win32::executor ex;
        auto job = win32::create_job_object();
        ex.register_job(job);

        for (auto &&c : tgt.commands) {
            visit(c, [&](auto &&c) {
                c.run(ex, [&] {
                });
                ex.run();
            });
        }
    }
};

}
