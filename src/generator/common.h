#pragma once

namespace sw {

namespace generator {

struct sw {
    static constexpr auto name = "sw"sv;
};

struct ninja {
    static constexpr auto name = "ninja"sv;

};

struct make {
    static constexpr auto name = "make"sv;
};

struct vs {
    static constexpr auto name = "vs"sv;
};

struct cmake {
    static constexpr auto name = "cmake"sv;
};

using generators = variant<sw,ninja,make,vs,cmake>;

} // namespace generator

using generators = generator::generators;

} // namespace sw
