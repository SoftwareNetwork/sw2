// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2022 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#ifdef __linux__
#include "../helpers/common.h"
#include "../sys/log.h"

#include <fcntl.h>
#include <signal.h>
#include <sys/epoll.h>
#include <sys/syscall.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>

// https://man7.org/linux/man-pages/man2/clone.2.html
struct clone_args {
    using u64 = uint64_t;

    u64 flags;        /* Flags bit mask */
    int *pidfd;        /* Where to store PID file descriptor
                                    (int *) */
    u64 child_tid;    /* Where to store child TID,
                                    in child's memory (pid_t *) */
    int *parent_tid;   /* Where to store child TID,
                                    in parent's memory (pid_t *) */
    u64 exit_signal;  /* Signal to deliver to parent on
                                    child termination */
    u64 stack;        /* Pointer to lowest byte of stack */
    u64 stack_size;   /* Size of stack */
    u64 tls;          /* Location of new TLS */
    u64 set_tid;      /* Pointer to a pid_t array
                                    (since Linux 5.5) */
    u64 set_tid_size; /* Number of elements in set_tid
                                    (since Linux 5.5) */
    u64 cgroup;       /* File descriptor for target cgroup
                                    of child (since Linux 5.7) */
};
int clone3(clone_args *args, size_t size) {
    return syscall(SYS_clone3, args, size);
}

namespace sw::linux {

struct executor {
    int efd;
    std::atomic_bool stopped{false};
    std::atomic_int jobs{0};
    std::map<int, std::move_only_function<void(char*,size_t)>> read_callbacks;
    std::map<int, std::move_only_function<void()>> process_callbacks;

    executor() {
        efd = epoll_create1(EPOLL_CLOEXEC);
        if (efd == -1) {
            throw std::runtime_error{"can create epoll"};
        }
    }
    ~executor() {
        //log_info("closing epoll");
        close(efd);
    }
    void run() {
        while (!stopped && (jobs || !process_callbacks.empty())) {
            run_one();
        }
    }
    void run_one() {
        epoll_event ev;
        if (epoll_wait(efd, &ev, 1, -1) == -1) {
            if (errno == EINTR) {
                return;
            }
            if (errno == EINVAL) {
                //log_info("epoll_wait");
                //return;
            }
            //throw std::runtime_error{"error epoll_wait"};
            perror("error epoll_wait");
            exit(1);
        }
        if (auto it = process_callbacks.find(ev.data.fd); it != process_callbacks.end()) {
            it->second();
            process_callbacks.erase(it);
            return;
        }
        auto it = read_callbacks.find(ev.data.fd);
        char buffer[4096];
        while (1) {
            auto count = read(ev.data.fd, buffer, sizeof(buffer));
            if (count <= -1) {
                if (errno == EINTR) {
                    continue;
                }
                //perror("read failed");
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
        epoll_event ev{};
        ev.events = EPOLLIN;
        ev.data.fd = fd;
        if (epoll_ctl(efd, EPOLL_CTL_ADD, fd, &ev) == -1) {
            throw std::runtime_error{"error epoll_ctl: " + std::to_string(errno)};
        }
        read_callbacks.emplace(fd, std::move(f));
    }
    void unregister_read_handle(auto &&fd) {
        read_callbacks.erase(fd);
    }
    void register_process(auto &&fd, auto &&f) {
        process_callbacks.emplace(fd, std::move(f));

        epoll_event ev{};
        ev.events = EPOLLIN | EPOLLONESHOT;
        ev.data.fd = fd;
        if (epoll_ctl(efd, EPOLL_CTL_ADD, fd, &ev) == -1) {
            throw std::runtime_error{"error epoll_ctl: " + std::to_string(errno)};
        }
    }
};

} // namespace sw::linux

namespace sw {

using linux::executor;

inline void debug_break() {
#if defined(__aarch64__)
    __asm__("brk #0x1"); // "trap" does not work for gcc
#else
    __asm__("int3");
#endif
}
inline bool is_debugger_attached() {
    char buf[4096];
    const int status_fd = ::open("/proc/self/status", O_RDONLY);
    if (status_fd == -1)
        return false;
    auto num_read = ::read(status_fd, buf, sizeof(buf) - 1);
    if (num_read <= 0)
        return false;
    buf[num_read] = '\0';
    constexpr char tracerPidString[] = "TracerPid:";
    const auto tracer_pid_ptr = ::strstr(buf, tracerPidString);
    if (!tracer_pid_ptr)
        return false;
    for (const char *characterPtr = tracer_pid_ptr + sizeof(tracerPidString) - 1; characterPtr <= buf + num_read;
         ++characterPtr) {
        if (::isspace(*characterPtr))
            continue;
        else
            return ::isdigit(*characterPtr) != 0 && *characterPtr != '0';
    }
    return false;
}
inline void debug_break_if_not_attached() {
    if (!is_debugger_attached()) {
        debug_break();
    }
}

} // namespace sw
#endif
