// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2022 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include "helpers/common.h"

namespace sw {

struct solution;

struct entry_point {
  std::function<void(solution &)> build;
  path source_dir;
  path binary_dir;

  void operator()(auto &sln, const auto &bs) {
    swap_and_restore sr{sln.source_dir};
    if (!source_dir.empty()) {
      sln.source_dir = source_dir;
    }
    swap_and_restore sr2{sln.binary_dir};
    if (!binary_dir.empty()) {
      sln.binary_dir = binary_dir;
    }
    swap_and_restore sr3{sln.bs, &bs};
    build(sln);
  }
};

struct direct_build_input {
  std::set<path> fns;
};

struct specification_file_input {
  path fn;
};

/// no input file, use some heuristics
struct directory_input {
  path dir;
};

/// only try to find spec file
struct directory_specification_file_input {};

using input = variant<specification_file_input, directory_input, direct_build_input>;

struct input_with_settings {
  input i;
  entry_point ep;
  /*std::set<build_settings> settings;

  void operator()(auto &sln) {
      for (auto &&s : settings) {
          ep(sln, s);
      }
  }*/
};

using inputs_type = std::vector<input_with_settings>;

// in current dir
// void detect_inputs(){}

} // namespace sw
