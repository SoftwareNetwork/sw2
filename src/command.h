#pragma once

#include "helpers.h"

#include <set>

#ifdef _WIN32
#include <windows.h>
#endif

namespace fs = std::filesystem;
using path = fs::path;

struct command {
    using string = std::string;
    std::vector<string> arguments;
    // arguments
    // working_directory
    // environment

    // exit_code
    // pid, pidfd

    // stdin,stdout,stderr

    //sync()
    //async()

    // execute?
    void start_job_object() {
        //ShellExecuteA(NULL, "open", "C:\\WINDOWS\\system32\\calc.exe", "/k ipconfig", 0, SW_SHOWNORMAL);
        //run_win32{}
    }
    void run_win32() {
#ifdef _WIN32
        /*
BOOL CreateProcessW(
  [in, optional]      LPCWSTR               lpApplicationName,
  [in, out, optional] LPWSTR                lpCommandLine,
  [in, optional]      LPSECURITY_ATTRIBUTES lpProcessAttributes,
  [in, optional]      LPSECURITY_ATTRIBUTES lpThreadAttributes,
  [in]                BOOL                  bInheritHandles,
  [in]                DWORD                 dwCreationFlags,
  [in, optional]      LPVOID                lpEnvironment,
  [in, optional]      LPCWSTR               lpCurrentDirectory,
  [in]                LPSTARTUPINFOW        lpStartupInfo,
  [out]               LPPROCESS_INFORMATION lpProcessInformation
);
        */
        DWORD flags = 0;
        STARTUPINFOW si = { 0 };
        PROCESS_INFORMATION pi = { 0 };
        LPVOID env = 0;
        LPCWSTR dir = 0;
        LPWSTR lpCommandLine = 0;

        std::wstring s;
        for (auto &&a : arguments) {
            s += path{a}.wstring() + L" ";
        }
        auto r = CreateProcessW(
            0,
            s.data(),
            0,
            0,
            TRUE, // bInheritHandles?
            flags,
            env,
            dir,
            &si,
            &pi
        );
        if (!r) {
            auto err = GetLastError();
            throw std::runtime_error("CreateProcessW failed, code = " + std::to_string(err));
        }
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

    void add(auto &&p) {
        arguments.push_back(p);
    }
    void add(const path &p) {
        arguments.push_back(p.string());
    }
    auto operator+=(auto &&arg) {
        add(arg);
        return appender{[&](auto &&v){operator+=(v);}};
    }
};

struct command2 : command {
    std::set<path> inputs;
    std::set<path> outputs;
};
