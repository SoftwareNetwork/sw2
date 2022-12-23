#pragma once

#include "helpers.h"

namespace sw {

struct solution;

struct entry_point {
    std::function<void(solution &)> entry_point;

    void operator()(auto &sln, const auto &bs) {
        sln.bs = &bs;
        entry_point(sln);
    }
};

} // namespace sw
