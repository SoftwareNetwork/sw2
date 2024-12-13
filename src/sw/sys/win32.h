// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2022 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include "../helpers/common.h"
#include "../sys/fs.h"
#include "exception.h"

#include <atomic>
#include <functional>
#include <map>
#include <stdexcept>
#include <string>

#ifndef FWD
#define FWD(x) std::forward<decltype(x)>(x)
#endif

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#ifdef byte
#undef byte
#endif

#define WINAPI_CALL(x) if (!(x)) {throw ::sw::win32::winapi_exception{#x};}
#define WINAPI_CALL_HANDLE(x) ::sw::win32::handle{x,[]{throw ::sw::win32::winapi_exception{"bad handle from " #x};}}

// we use this for easy command line building/bootstrapping
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "OleAut32.lib")

namespace sw::win32 {

struct winapi_exception : std::runtime_error {
    template <typename F>
    struct scope_exit {
        F &&f;
        ~scope_exit() {
            f();
        }
    };

    using base = std::runtime_error;
    winapi_exception(const string &msg) : base{msg + ": "s + get_last_error()} {
    }
    string get_last_error() const {
        auto code = GetLastError();

        LPVOID lpMsgBuf;
        FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL,
                      code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&lpMsgBuf, 0, NULL);
        scope_exit se{[&]{LocalFree(lpMsgBuf);}};
        string msg = (const char *)lpMsgBuf;

        return "error code = "s + std::to_string(code) + ": " + msg;
    }
};

struct handle {
    HANDLE h{INVALID_HANDLE_VALUE};

    handle() = default;
    handle(HANDLE h, auto &&err) : h{h} {
        if (h == INVALID_HANDLE_VALUE || !h) {
            err();
        }
    }
    handle(HANDLE h) : handle{h,[]{throw winapi_exception{"bad handle"};}} {
    }
    handle(const handle &) = delete;
    handle &operator=(const handle &) = delete;
    handle(handle &&rhs) noexcept {
        operator=(std::move(rhs));
    }
    handle &operator=(handle &&rhs) noexcept {
        this->~handle();
        h = rhs.h;
        rhs.h = INVALID_HANDLE_VALUE;
        return *this;
    }
    ~handle() {
        reset();
    }

    operator HANDLE() & { return h; }
    operator HANDLE() && { return release(); }
    operator HANDLE*() { return &h; }

    void reset() {
        CloseHandle(h);
        h = INVALID_HANDLE_VALUE;
    }
    HANDLE release() {
        auto hold = h;
        h = INVALID_HANDLE_VALUE;
        return hold;
    }
};

auto create_job_object() {
    handle job = CreateJobObject(0, 0);
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION ji;
    WINAPI_CALL(QueryInformationJobObject(job, JobObjectExtendedLimitInformation, &ji, sizeof(ji), 0));
    ji.BasicLimitInformation.LimitFlags |= JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    WINAPI_CALL(SetInformationJobObject(job, JobObjectExtendedLimitInformation, &ji, sizeof(ji)));
    return job;
}
HANDLE default_job_object() {
    static auto job = []() {
        // we do release here, otherwise process will be terminated with 0 exit code
        // on CloseHandle during normal exit process
        auto job = create_job_object().release();
        /*BOOL injob{false};
        // check if we are a child in some job already
        // injob is probably true under debugger/visual studio
        WINAPI_CALL(IsProcessInJob(GetCurrentProcess(), 0, &injob));*/
        // assign this process to a job
        if (1 /*|| !injob*/) {
            WINAPI_CALL(AssignProcessToJobObject(job, GetCurrentProcess()));
        }
        return job;
    }();
    return job;
}

struct pipe {
    handle r, w;

    pipe() = default;
    pipe(bool inherit) {
        SW_UNIMPLEMENTED;
        //init_write(inherit);
    }
    auto pipe_id() {
        static std::atomic_int pipe_id{0};
        return pipe_id++;
    }
    auto pipe_name() {
        return std::format(L"\\\\.\\pipe\\swpipe.{}.{}", GetCurrentProcessId(), pipe_id());
    }
    void init(bool inherit_read, bool overlapped_read, bool inherit_write, bool overlapped_write) {
        auto s = pipe_name();

        SECURITY_ATTRIBUTES sar{};
        sar.bInheritHandle = !!inherit_read;
        DWORD buffer_size{};
        r = WINAPI_CALL_HANDLE(CreateNamedPipeW(s.c_str(), PIPE_ACCESS_INBOUND | (overlapped_read ? FILE_FLAG_OVERLAPPED : 0), 0, 1, buffer_size, buffer_size, 0, &sar));

        SECURITY_ATTRIBUTES saw{};
        saw.bInheritHandle = !!inherit_write;
        w = WINAPI_CALL_HANDLE(CreateFileW(s.c_str(), GENERIC_WRITE, 0, &saw, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | (overlapped_write ? FILE_FLAG_OVERLAPPED : 0), 0));
    }
    void init_double(bool inherit = false) {
        SECURITY_ATTRIBUTES sa{};
        sa.bInheritHandle = !!inherit;

        auto s = pipe_name();
        DWORD buffer_size{};
        r = WINAPI_CALL_HANDLE(CreateNamedPipeW(s.c_str(), PIPE_ACCESS_INBOUND, 0, 1, buffer_size, buffer_size, 0, &sa));
        w = WINAPI_CALL_HANDLE(CreateFileW(s.c_str(), GENERIC_WRITE, 0, &sa, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0));
    }
};

struct io_callback {
    OVERLAPPED o{};
    std::move_only_function<void(size_t)> f;
    uint8_t *buf{};
    size_t bufsize;
    size_t datasize{};
    DWORD last_error{1}; // default error code

    io_callback(size_t bufsize = 4096) : bufsize{bufsize} {
        buf = new uint8_t[bufsize];
    }
    ~io_callback() {
        delete [] buf;
    }
    auto data() const {return buf;}
    auto size() const {return datasize;}
};

struct executor {
    handle port;
    std::atomic_bool stopped{false};
    std::atomic_int jobs{0};
    std::map<DWORD, std::move_only_function<void(bool)>> process_callbacks;
    handle job;

    executor() {
        port = CreateIoCompletionPort(INVALID_HANDLE_VALUE, 0, 0, 0);
        job = win32::create_job_object();
        register_job(job);
    }
    void stop() {
        stopped = true;
        PostQueuedCompletionStatus(port, 0, 0, 0);
    }
    void run() {
        while (!stopped && (jobs || !process_callbacks.empty())) {
            run_one();
        }
    }
    void run_one() {
        DWORD bytes;
        ULONG_PTR key;
        io_callback *o{nullptr};
        if (!GetQueuedCompletionStatus(port, &bytes, &key, (LPOVERLAPPED *)&o, 1000)) {
            auto err = GetLastError();
            if (0) {
            } else if (err == ERROR_BROKEN_PIPE) {
            } else if (err != WAIT_TIMEOUT) {
                throw std::runtime_error{"cannot get io completion status"};
            } else {
                return;
            }
        }
        if (key == (ULONG_PTR)port.h) {
            switch (bytes) {
            case JOB_OBJECT_MSG_NEW_PROCESS:
                break;
            case JOB_OBJECT_MSG_ACTIVE_PROCESS_ZERO:
                if (process_callbacks.size() != 0) {
                    throw std::logic_error{"bad process_callbacks, you did not call anything"};
                }
                (uint64_t &)o = process_callbacks.begin()->first;
            case JOB_OBJECT_MSG_EXIT_PROCESS:
                if (auto it = process_callbacks.find(static_cast<DWORD>((uint64_t)o)); it != process_callbacks.end()) {
                    it->second(false);
                    process_callbacks.erase(it);
                }
                break;
            case JOB_OBJECT_MSG_ABNORMAL_EXIT_PROCESS:
                if (auto it = process_callbacks.find(static_cast<DWORD>((uint64_t)o)); it != process_callbacks.end()) {
                    it->second(false);
                    process_callbacks.erase(it);
                }
                break;
            case JOB_OBJECT_MSG_END_OF_PROCESS_TIME:
                if (auto it = process_callbacks.find(static_cast<DWORD>((uint64_t)o)); it != process_callbacks.end()) {
                    it->second(true);
                    process_callbacks.erase(it);
                }
                break;
            default:
                throw std::runtime_error{"unhandled process completion status"};
            }
            return;
        }
        if (!o) {
            return; // stop signal?
        }
        --jobs;
        o->f(bytes);
        //delete o;
    }

    void register_handle(auto &&h) {
        WINAPI_CALL(CreateIoCompletionPort(h, port, 0, 0));
    }
    void register_job(auto &&job) {
        JOBOBJECT_ASSOCIATE_COMPLETION_PORT jcp{};
        jcp.CompletionPort = port;
        jcp.CompletionKey = (PVOID)port.h;
        WINAPI_CALL(SetInformationJobObject(job, JobObjectAssociateCompletionPortInformation, &jcp, sizeof(jcp)));
    }
    void register_process(auto &&h, auto &&pid, auto &&f) {
        WINAPI_CALL(AssignProcessToJobObject(job, h));
        process_callbacks.emplace(pid, FWD(f));
    }

    void read_async(auto &&h, auto &&user_f) {
        auto cb = new io_callback;
        cb->f = [cb, user_f = std::move(user_f)](size_t sz) mutable {
            cb->datasize = sz;
            if (sz == 0) {
                user_f(cb, std::error_code(cb->last_error, std::generic_category()));
                delete cb;
            } else {
                user_f(cb, std::error_code{});
            }
        };
        read_async(h, cb);
    }
    void read_async(auto &&h, io_callback *cb) {
        cb->o = OVERLAPPED{};
        ++jobs;
        DWORD bytes_read;
        if (!ReadFile(h, cb->buf, cb->bufsize, &bytes_read, (OVERLAPPED *)cb)) {
            auto err = GetLastError();
            if (err != ERROR_IO_PENDING) {
                --jobs;
                cb->last_error = err;
                cb->f(0);
            }
        }
    }

    void write_async(auto &&h, void *data, size_t sz, auto &&user_f) {
        auto cb = new io_callback;
        cb->f = [cb, user_f = std::move(user_f)](size_t sz) mutable {
            cb->datasize = sz;
            if (sz == 0) {
                user_f(cb, std::error_code(cb->last_error, std::generic_category()));
                delete cb;
            } else {
                user_f(cb, std::error_code{});
            }
        };
        write_async(h, cb, data, sz);
    }
    void write_async(auto &&h, io_callback *cb, void *data, size_t sz) {
        cb->o = OVERLAPPED{};
        auto tocopy = std::min(sz, cb->bufsize);
        memcpy(cb->buf, data, tocopy);
        ++jobs;
        if (!WriteFile(h, cb->buf, tocopy, 0, (OVERLAPPED *)cb)) {
            auto err = GetLastError();
            if (err != ERROR_IO_PENDING) {
                --jobs;
                cb->last_error = err;
                cb->f(0);
            }
        }
    }
};

} // namespace sw::win32

namespace sw {

using win32::executor;

inline void debug_break() {
    DebugBreak();
}
inline bool is_debugger_attached() {
    return IsDebuggerPresent();
}
inline void debug_break_if_not_attached() {
    if (!is_debugger_attached()) {
        debug_break();
    }
}

} // namespace sw

//#undef WINAPI_CALL

#endif
