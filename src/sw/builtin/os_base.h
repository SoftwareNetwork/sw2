// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2022 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include "../helpers/common.h"

namespace sw {

namespace os {

struct windows {
    static constexpr auto name = "windows"sv;

    static constexpr auto executable_extension = ".exe";
    static constexpr auto object_file_extension = ".obj";
    static constexpr auto static_library_extension = ".lib";
    static constexpr auto shared_library_extension = ".dll";

    static bool is(string_view sv) {
        return name == sv;
    }

    // deps:
    // kernel32 dependency (winsdk.um)
};

struct mingw : windows {
    static constexpr auto name = "mingw"sv;

    static bool is(string_view sv) {
        return name == sv;
    }
};

struct cygwin : windows {
    static constexpr auto name = "cygwin"sv;

    static constexpr auto static_library_extension = ".a";
    static constexpr auto object_file_extension = ".o";

    static bool is(string_view sv) {
        return name == sv;
    }
};

struct unix {
    static constexpr auto object_file_extension = ".o";
    static constexpr auto static_library_extension = ".a";
};

struct linux : unix {
    static constexpr auto name = "linux"sv;

    static constexpr auto shared_library_extension = ".so";

    static bool is(string_view sv) {
        return name == sv;
    }
};

struct darwin : unix {
    static constexpr auto shared_library_extension = ".dylib";
};

struct macos : darwin {
    static constexpr auto name = "macos"sv;

    static bool is(string_view sv) {
        return name == sv;
    }
};
// ios etc

struct wasm : unix {
    static constexpr auto name = "wasm"sv;

    static constexpr auto executable_extension = ".html";

    static bool is(string_view sv) {
        return name == sv;
    }
};

} // namespace os

namespace build_type {

struct debug {
    static constexpr auto name = "debug"sv;
    static constexpr auto short_name = "d"sv;

    static bool is(string_view sv) {
        return name == sv || short_name == sv;
    }
};
struct minimum_size_release {
    static constexpr auto name = "minimum_size_release"sv;
    static constexpr auto short_name = "msr"sv;

    static bool is(string_view sv) {
        return name == sv || short_name == sv;
    }
};
struct release_with_debug_information {
    static constexpr auto name = "release_with_debug_information"sv;
    static constexpr auto short_name = "rwdi"sv;

    static bool is(string_view sv) {
        return name == sv || short_name == sv;
    }
};
struct release {
    static constexpr auto name = "release"sv;
    static constexpr auto short_name = "r"sv;

    static bool is(string_view sv) {
        return name == sv || short_name == sv;
    }
};

} // namespace build_type

namespace library_type {

struct static_ {
    static constexpr auto name = "static"sv;
    static constexpr auto short_name = "st"sv;
};
struct shared {
    static constexpr auto name = "shared"sv;
    static constexpr auto short_name = "sh"sv;
};

} // namespace library_type

} // namespace sw
