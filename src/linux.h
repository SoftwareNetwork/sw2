#pragma once

#include "helpers.h"

#ifdef __linux__
#include <signal.h>
#include <sys/epoll.h>
#include <sys/syscall.h>
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
#endif

namespace sw::linux {

struct executor {
    int efd;
    std::atomic_bool stopped{false};
    std::atomic_int jobs{0};
    std::map<int, std::move_only_function<void()>> process_callbacks;

    executor() {
        efd = epoll_create1(EPOLL_CLOEXEC);
        if (efd == -1) {
            throw std::runtime_error{"can create epoll"};
        }
    }
    ~executor() {
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
            throw std::runtime_error{"error epoll_wait"};
        }
        if (auto it = process_callbacks.find(ev.data.fd); it != process_callbacks.end()) {
            it->second();
            process_callbacks.erase(it);
            return;
        }
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
}
