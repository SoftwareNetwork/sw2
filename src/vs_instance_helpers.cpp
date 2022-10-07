// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2022 Egor Pugin <egor.pugin@gmail.com>

#include "vs_instance_helpers.h"
#include "cmVSSetupHelper.h"

#include <stdexcept>

std::vector<VSInstanceInfo> enumerate_vs_instances() {
#ifdef _WIN32
    CoInitializeEx(0,0);

    sw::cmVSSetupAPIHelper h;
    if (!h.EnumerateVSInstances()) {
        throw std::runtime_error("can't enumerate vs instances");
    }
    return h.instances;
#else
    return {};
#endif
}
