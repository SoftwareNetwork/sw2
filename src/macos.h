// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2022 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#ifdef __APPLE__
#include "helpers.h"

namespace sw::macos {

struct executor {
};

} // namespace sw::macos

namespace sw {

using macos::executor;

}

#endif
