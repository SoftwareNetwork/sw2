// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2024 Egor Pugin <egor.pugin@gmail.com>

#include "command/command.h"
#include "input.h"
#include "sw/builtin/vs_instance_helpers.h"

namespace sw {

struct sw_script {
  struct file {
    path fn;
    std::vector<path> include_dirs;
  };

  std::vector<file> files;
  std::vector<path> include_dirs;
  bool debug{};

  // returns command
  void build() {
    //g++ src/client.cpp -Isrc -std=c++26 -g -O0 -lole32 -lOleAut32" -static-libstdc++ -static-libgcc -static -lpthread
    //clang src\client.cpp -Isrc -std=c++26 -g -O0
    //cl src\client.cpp /std:c++latest /nologo /EHsc -Isrc // has issues

#ifndef NDEBUG
    debug = 1;
#endif

    auto common_flags = [&](auto &&cmd) {
      for (auto &&f : files) {
        cmd += f.fn;
      }
      cmd += "-std=c++26";
      // -MD
      if (debug) {
        cmd += "-g", "-O0";
      }
      for (auto &&f : files) {
        for (auto &&i : f.include_dirs) {
          cmd += "-I", i;
        }
      }
      for (auto &&i : include_dirs) {
        cmd += "-I", i;
      }
    };

    if (auto gpp = resolve_executable("g++"); gpp && 0) {
      gcc_command cmd;
      cmd += *gpp;
      common_flags(cmd);
      cmd += "-lole32", "-lOleAut32";
      cmd += "-static-libstdc++", "-static-libgcc", "-static", "-lpthread";
      cmd();
      // clang is faster than mingw atm
    } else if (auto clang = resolve_executable("clang"); clang) {
      gcc_command cmd;
      cmd += *clang;
      common_flags(cmd);
      cmd();
    } else {
      // try msvc
      auto inst = enumerate_vs_instances();
      SW_UNIMPLEMENTED;
    }
  }
};

struct build {
  inputs_type inputs;
  // list of repos

  void run() {
    sw_script ss;
    for (auto &&i : inputs) {
      visit(
          i.i,
          [&](specification_file_input &v) {
            ss.files.emplace_back(v.fn);
          },
          [&](directory_input &v) {
            SW_UNIMPLEMENTED;
            int a = 5;
            a++;
          },
          [&](direct_build_input &v) {
            SW_UNIMPLEMENTED;
            int a = 5;
            a++;
          });
    }
    ss.build();
  }
};

} // namespace sw
