// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2022 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include "helpers.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace sw::win32 {

struct io_callback {
    OVERLAPPED o{};
    std::move_only_function<void(size_t)> f;
    string buf;
};

struct handle {
    HANDLE h{INVALID_HANDLE_VALUE};

    handle() = default;
    handle(HANDLE h, auto &&err) : h{h} {
        if (h == INVALID_HANDLE_VALUE || !h) {
            err();
        }
    }
    handle(HANDLE h) : handle{h,[]{throw std::runtime_error{"bad handle"};}} {
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
        if (!CloseHandle(h)) {
        }
    }

    operator HANDLE() { return h; }
    operator HANDLE*() { return &h; }

    void reset() {
        CloseHandle(h);
        h = INVALID_HANDLE_VALUE;
    }
};

auto create_job_object() {
    handle job = CreateJobObject(0, 0);
    /*if (!AssignProcessToJobObject(job, GetCurrentProcess())) {
        throw std::runtime_error{"cannot AssignProcessToJobObject"};
    }*/

    JOBOBJECT_EXTENDED_LIMIT_INFORMATION ji;
    if (!QueryInformationJobObject(job, JobObjectExtendedLimitInformation, &ji, sizeof(ji), 0)) {
        throw std::runtime_error{"cannot QueryInformationJobObject"};
    }
    ji.BasicLimitInformation.LimitFlags |= JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (!SetInformationJobObject(job, JobObjectExtendedLimitInformation, &ji, sizeof(ji))) {
        throw std::runtime_error{"cannot SetInformationJobObject"};
    }
    return job;
}
HANDLE default_job_object() {
    static handle job = create_job_object();
    return job;
}

struct pipe {
    static inline std::atomic_int pipe_id{0};
    handle r, w;

    pipe() = default;
    pipe(bool inherit) {
        init(inherit);
    }
    void init(bool inherit = false) {
        DWORD sz = 0;
        auto s = std::format(L"\\\\.\\pipe\\swpipe.{}.{}", GetCurrentProcessId(), pipe_id++);
        r = CreateNamedPipeW(s.c_str(), PIPE_ACCESS_INBOUND | FILE_FLAG_OVERLAPPED, 0, 1, sz, sz, 0, 0);

        SECURITY_ATTRIBUTES sa = {0};
        sa.bInheritHandle = !!inherit;
        w = CreateFileW(s.c_str(), GENERIC_WRITE, 0, &sa, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, 0);
    }
};

struct executor {
    handle port;
    std::atomic_bool stopped{false};
    std::atomic_int jobs{0};
    std::map<DWORD, std::move_only_function<void()>> process_callbacks;
    HANDLE job{INVALID_HANDLE_VALUE};

    executor() {
        port = CreateIoCompletionPort(INVALID_HANDLE_VALUE, 0, 0, 0);
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
        if (key == 10) {
            switch (bytes) {
            case JOB_OBJECT_MSG_NEW_PROCESS:
                break;
            case JOB_OBJECT_MSG_EXIT_PROCESS:
                if (auto it = process_callbacks.find(static_cast<DWORD>((uint64_t)o)); it != process_callbacks.end()) {
                    it->second();
                    process_callbacks.erase(it);
                }
                break;
            default:
                break;
            }
            return;
        }
        if (!o) {
            return; // stop signal?
        }
        --jobs;
        o->f(bytes);
        delete o;
    }

    void register_handle(auto &&h) {
        if (!CreateIoCompletionPort(h, port, 0, 0)) {
            throw std::runtime_error{"cannot add fd to port"};
        }
    }
    void register_job(auto &&job) {
        JOBOBJECT_ASSOCIATE_COMPLETION_PORT jcp{};
        jcp.CompletionPort = port;
        jcp.CompletionKey = (void *)10;
        if (!SetInformationJobObject(job, JobObjectAssociateCompletionPortInformation, &jcp, sizeof(jcp))) {
            throw std::runtime_error{"cannot SetInformationJobObject"};
        }
        this->job = job;
    }
    void register_process(auto &&h, auto &&pid, auto &&f) {
        if (!AssignProcessToJobObject(job, h)) {
            throw std::runtime_error{"cannot AssignProcessToJobObject"};
        }
        process_callbacks.emplace(pid, FWD(f));
    }

    void read_async(auto &&h, auto &&f) {
        auto cb = new io_callback;
        cb->buf.resize(4096);
        cb->f = [cb, f = std::move(f)](size_t sz) mutable {
            if (sz == 0) {
                f(cb->buf, std::error_code(1, std::generic_category()));
                return;
            }
            cb->buf.resize(sz);
            f(cb->buf, std::error_code{});
        };
        ++jobs;
        if (!ReadFile(h, cb->buf.data(), cb->buf.size(), 0, (OVERLAPPED *)cb)) {
            auto err = GetLastError();
            if (err != ERROR_IO_PENDING) {
                --jobs;
                f(cb->buf, std::error_code(err, std::generic_category()));
                delete cb;
                return;
            }
        }
    }
};

} // namespace sw::win32

namespace sw {

using win32::executor;

}

#endif
