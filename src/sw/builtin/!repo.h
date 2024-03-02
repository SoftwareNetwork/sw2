#pragma once

#include "msvc.h"

namespace sw {

struct builtin_repository {
    void init(auto &&swctx) {
        msvc m;
        m.detect(swctx);
    }
};

}
