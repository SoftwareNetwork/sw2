#pragma once

#include "helpers.h"

namespace sw {

struct solution;

struct entry_point {
    std::function<void(solution &)> f;

    void operator()(auto &sln, const auto &bs) {
        sln.bs = &bs;
        f(sln);
    }
};

} // namespace sw
