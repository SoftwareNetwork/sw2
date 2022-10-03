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
        CloseHandle(h);
    }

    operator HANDLE() const { return h; }
    operator PHANDLE() { return &h; }
};

struct pipe {
    handle r,w;

    pipe(bool inherit = false) {
        SECURITY_ATTRIBUTES sa = {0};
        sa.bInheritHandle = !!inherit;
        if (!CreatePipe(r, w, &sa, 0)) {
            throw std::runtime_error{"cannot create pipe"};
        }
    }
};

struct executor {
    HANDLE port;

    executor() {
        port = CreateIoCompletionPort(INVALID_HANDLE_VALUE, 0, 0, 0);
        if (!port) {
            throw std::runtime_error{"cannot create io port"};
        }
    }
    ~executor() {
        CloseHandle(port);
    }
};

}
