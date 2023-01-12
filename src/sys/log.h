// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2022 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include "mmap.h"

namespace sw {

struct log {
    mmap_file<char> f;

    log(const path &fn) : f{fn} {
    }
};

void log() {
}
struct log_proxy {
    std::string s;
    log_proxy &operator<<(auto &&v) {

        return *this;
    }
};

// add logger arg?
#define log_debug(x) log_proxy{} << x

} // namespace sw
