#pragma once

#include "../helpers/common.h"

namespace sw {

namespace generator {

struct sw {
    static constexpr auto name = "sw"sv;

    void generate(auto &&sln, auto &&command_executor) {
        string s;
        auto &cmds = command_executor.external_commands;
        for (auto &&c : cmds) {
            visit(*c, [&](auto &&c) {
                // round trip commands here
                s += c.print() + "\n";
            });
        }
        write_file(sln.work_dir / "g" / name / "commands.txt", s);
        SW_UNIMPLEMENTED;
    }
};

struct ninja {
    static constexpr auto name = "ninja"sv;

    void generate(auto &&sln, auto &&command_executor) {
        SW_UNIMPLEMENTED;
    }
};

struct make {
    static constexpr auto name = "make"sv;
};

struct vs {
    static constexpr auto name = "vs"sv;

    void generate(auto &&sln, auto &&command_executor) {
        SW_UNIMPLEMENTED;
    }
};

struct cmake {
    static constexpr auto name = "cmake"sv;
};

// more?
// waf meson

using generators = variant<sw,ninja,vs>;

} // namespace generator

using generators = generator::generators;

} // namespace sw
