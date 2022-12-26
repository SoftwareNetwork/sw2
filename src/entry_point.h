#pragma once

#include "helpers.h"

namespace sw {

struct solution;

struct entry_point {
    std::function<void(solution &)> f;
    path source_dir;

    void operator()(auto &sln, const auto &bs) {
        sln.bs = &bs;
        auto oldsdir = sln.source_dir;
        if (!source_dir.empty()) {
            sln.source_dir = source_dir;
        }
        f(sln);
        sln.source_dir = oldsdir;
    }
};

} // namespace sw
