#pragma once

#include "helpers.h"

#ifdef _WIN32
#include <windows.h>
#endif

namespace sw::win32 {

struct handle {
    HANDLE h{INVALID_HANDLE_VALUE};

    handle() = default;
    handle(HANDLE h) : h{h} {
        if (h == INVALID_HANDLE_VALUE) {
            throw std::runtime_error{"bad handle"};
        }
    }
    handle(const handle &) = delete;
    handle &operator=(const handle &) = delete;
    handle(handle &&rhs) noexcept {
        operator=(std::move(rhs));
    }
    handle &operator=(handle &&rhs) noexcept {
        h = rhs.h;
        rhs.h = INVALID_HANDLE_VALUE;
        return *this;
    }
    ~handle() {
        if (!CloseHandle(h)) {
            int a = 5;
        }
    }

    operator HANDLE() { return h; }
    operator HANDLE*() { return &h; }

    void reset() {
        CloseHandle(h);
        h = INVALID_HANDLE_VALUE;
    }
};

struct io_callback {
    OVERLAPPED o{};
    std::function<void(size_t)> f;
    std::vector<uint8_t> buf;
};

HANDLE default_job_object() {
    static handle job = []() {
        auto job = CreateJobObject(0, 0);
        if (!job) {
            throw std::runtime_error{"cannot CreateJobObject"};
        }
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
    }();
    return job;
}

template <typename Executor>
struct pipe {
    static inline std::atomic_int pipe_id{0};
    handle r, w;
    Executor &ex;

    pipe(auto &&ex, bool inherit = false) : ex{ex} {
        init(inherit);
        if (!CreateIoCompletionPort(r, ex.port, 0, 0)) {
            throw std::runtime_error{"cannot add fd to port"};
        }
    }
    void init(bool inherit) {
        DWORD sz = 0;
        auto s = std::format(L"\\\\.\\pipe\\swpipe.{}.{}", GetCurrentProcessId(), pipe_id++);
        r = CreateNamedPipeW(s.c_str(), PIPE_ACCESS_INBOUND | FILE_FLAG_OVERLAPPED, 0, 1, sz, sz, 0, 0);

        SECURITY_ATTRIBUTES sa = {0};
        sa.bInheritHandle = !!inherit;
        w = CreateFileW(s.c_str(), GENERIC_WRITE, 0, &sa, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, 0);
    }

    void read_async(auto &&f) {
        auto cb = new io_callback;
        cb->buf.resize(4096);
        cb->f = [cb, f](size_t sz) mutable {
            if (sz == 0) {
                f(cb->buf, std::error_code(1, std::generic_category()));
                return;
            }
            cb->buf.resize(sz);
            f(cb->buf, std::error_code{});
        };
        ++ex.jobs;
        if (!ReadFile(r, cb->buf.data(), cb->buf.size(), 0, (OVERLAPPED*)cb)) {
            auto err = GetLastError();
            if (err != ERROR_IO_PENDING) {
                --ex.jobs;
                f(cb->buf, std::error_code(err, std::generic_category()));
                return;
            }
        }
    }
};

struct executor {
    handle port;
    std::atomic_bool stopped{false};
    std::atomic_int jobs{0};

    executor() {
        port = CreateIoCompletionPort(INVALID_HANDLE_VALUE, 0, 0, 0);
    }
    void stop() {
        stopped = true;
        PostQueuedCompletionStatus(port, 0, 0, 0);
    }
    void run() {
        while (!stopped && jobs) {
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
            int a = 5;
            switch (bytes) {
            case JOB_OBJECT_MSG_NEW_PROCESS:
                a++;
                break;
            case JOB_OBJECT_MSG_EXIT_PROCESS:
                a++;
                break;
            default:
                a++;
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
};

}
