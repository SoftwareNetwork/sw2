// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2022 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#ifdef __APPLE__
#include "helpers.h"

#include <spawn.h>
#include <sys/event.h>
#include <unistd.h>

extern char **environ;

namespace sw::macos {

struct executor {
    int kfd;
    std::atomic_bool stopped{false};
    std::atomic_int jobs{0};
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
        char buffer[4096];
        while (1) {
            auto count = read(ev.ident, buffer, sizeof(buffer));
            if (count == -1) {
                if (errno == EINTR) {
                    continue;
                } else {
                    perror("read");
                    exit(1);
                }
            } else if (count == 0) {
                break;
            } else {
                //handle_child_process_output(buffer, count);
                int a = 5;
                a++;
            }
        }
    }

    void register_read_handle(auto &&fd) {
        struct kevent ev{};
        if (kevent(kfd, &ev, 1, 0, 0, 0) == -1) {
            throw std::runtime_error{"error kevent queue"};
        }
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
#ifdef _WIN32
#elif defined(__aarch64__)
    __asm__("brk #0x1"); // "trap" does not work for gcc
#else
    __asm__("int3");
#endif
}

}
#endif
