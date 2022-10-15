// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2022 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include "helpers.h"

namespace sw {

namespace os {

struct windows {
    // kernel32 dependency (winsdk.um)
};

} // namespace os

namespace arch {

struct x86 {};
struct x64 {};
using amd64 = x64;
using x86_64 = x64;

struct arm64 {};
using aarch64 = arm64;

} // namespace arch

} // namespace sw
