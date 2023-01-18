// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2022 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#ifdef __APPLE__
#include "../helpers/common.h"

#include <spawn.h>
#include <sys/event.h>
#include <unistd.h>
#include <sys/sysctl.h>
#include <signal.h>

extern char **environ;

namespace sw::macos {

struct executor {
    int kfd;
    std::atomic_bool stopped{false};
    std::atomic_int jobs{0};
    std::map<int, std::move_only_function<void(char*,size_t)>> read_callbacks;
    std::map<int, std::move_only_function<void()>> process_callbacks;

    executor() {
        kfd = kqueue();
        if (kfd == -1) {
            throw std::runtime_error{"can create kqueue"};
        }
    }
    ~executor() {
        close(kfd);
    }
    void run() {
        while (!stopped && (jobs || !process_callbacks.empty())) {
            run_one();
        }
    }
    void run_one() {
        struct kevent ev{};
        if (kevent(kfd, 0, 0, &ev, 1, 0) == -1) {
            throw std::runtime_error{"error kevent queue"};
        }
        if (auto it = process_callbacks.find(ev.ident); it != process_callbacks.end()) {
            it->second();
            process_callbacks.erase(it);
            return;
        }
        auto it = read_callbacks.find(ev.ident);
        char buffer[4096];
        while (1) {
            auto count = read(ev.ident, buffer, sizeof(buffer));
            if (count <= -1) {
                if (errno == EINTR) {
                    continue;
                }
                //perror("read");
                //exit(1);
                break;
            } else if (count == 0) {
                break;
            }
            if (it != read_callbacks.end()) {
                it->second(buffer, count);
            }
        }
    }

    void register_read_handle(auto &&fd, auto &&f) {
        struct kevent ev{};
        EV_SET(&ev, fd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, 0);
        if (kevent(kfd, &ev, 1, 0, 0, 0) == -1) {
            throw std::runtime_error{"error kevent queue"};
        }
        read_callbacks.emplace(fd, std::move(f));
    }
    void unregister_read_handle(auto &&fd) {
        read_callbacks.erase(fd);
    }
    void register_process(auto &&pid, auto &&f) {
        struct kevent ev{};
        EV_SET(&ev, pid, EVFILT_PROC, EV_ADD | EV_ENABLE, NOTE_EXIT, 0, 0);
        if (kevent(kfd, &ev, 1, 0, 0, 0) == -1) {
            if (errno == ESRCH) {
                f();
                return;
            }
            throw std::runtime_error{"error kevent queue"};
        }
        process_callbacks.emplace(pid, std::move(f));
    }
};

} // namespace sw::macos

namespace sw {

using macos::executor;

inline void debug_break() {
#if defined(__aarch64__)
    __asm__("brk #0x1"); // "trap" does not work for gcc
#else
    __asm__("int3");
#endif
}
inline bool is_debugger_attached() {
    int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PID, getpid()};
    struct kinfo_proc info{};
    auto size = sizeof(info);
    sysctl(mib, sizeof(mib) / sizeof(*mib), &info, &size, NULL, 0);
    // check sysctl result?
    return ((info.kp_proc.p_flag & P_TRACED) != 0);
}
inline void debug_break_if_not_attached() {
    if (!is_debugger_attached()) {
        debug_break();
    }
}

} // namespace sw
#endif
