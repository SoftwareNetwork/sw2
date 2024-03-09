#pragma once

#include "msvc.h"

namespace sw {

struct builtin_repository {
    void init(auto &&swctx) {
        // in local mode we should work only with c/c++ compilers
        // or maybe we should detect only basic compilers to boostrap ourselves/repositories
        msvc m;
        m.detect(swctx);
    }
};

}
