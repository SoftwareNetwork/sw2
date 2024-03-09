#pragma once

#include "../helpers/common.h"

namespace sw {

namespace arch {

struct x86 {
    static constexpr auto name = "x86"sv;
    static constexpr auto clang_target_name = "i586"sv; // but also 386, 586, 686

    static bool is(string_view sv) {
        return name == sv;
    }
    auto operator<=>(const x86 &) const = default;
};
struct x64 {
    static constexpr auto name = "x64"sv;
    static constexpr auto name1 = "x86_64"sv;
    static constexpr auto name2 = "amd64"sv;
    static constexpr auto clang_target_name = name1;

    static bool is(string_view sv) {
        return 0 || sv == name || sv == name1 || sv == name2;
    }
    auto operator<=>(const x64 &) const = default;
};
using amd64 = x64;
using x86_64 = x64;

struct arm {
    static constexpr auto name = "arm"sv;
    static constexpr auto clang_target_name = name;

    static bool is(string_view sv) {
        return name == sv;
    }
    auto operator<=>(const arm &) const = default;
};
struct arm64 {
    static constexpr auto name = "arm64"sv;
    static constexpr auto clang_target_name = name;

    static bool is(string_view sv) {
        return name == sv;
    }
    auto operator<=>(const arm64 &) const = default;
};
using aarch64 = arm64; // give alternative names

} // namespace arch

using arch_type = special_variant<arch::x86, arch::x64, arch::arm, arch::arm64>;

arch_type current_arch() {
#if defined(__x86_64__) || defined(_M_X64)
    return arch::x64{};
#elif defined(__i386__) || defined(_M_IX86)
    return arch::x86{};
#elif defined(__arm64__) || defined(__aarch64__) || defined(_M_ARM64)
    return arch::arm64{};
#elif defined(__arm__) || defined(_M_ARM)
    return arch::arm{};
#else
#error "unknown arch"
#endif
}

auto get_windows_arch_name(const arch_type &a) {
    return a.visit([&](const auto &a) {
        return std::string{a.name};
    });
}

}
