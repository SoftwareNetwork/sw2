// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2022 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include "os.h"
#include "entry_point.h"

namespace sw {

struct specification_file_input {
    path fn;
};

/// no input file, use some heuristics
struct directory_input {
    path dir;
};

/// only try to find spec file
struct directory_specification_file_input {};

using input = variant<specification_file_input, directory_input>;

struct input_with_settings {
    entry_point ep;
    std::set<build_settings> settings;

    void operator()(auto &sln) {
        for (auto &&s : settings) {
            ep(sln, s);
        }
    }
};

// in current dir
//void detect_inputs(){}

} // namespace sw
