// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2022 Egor Pugin <egor.pugin@gmail.com>

#include "command.h"
#include "storage.h"
#include "vs_instance_helpers.h"
#include "package.h"
#include "detect.h"
#include "rule_target.h"

#include <map>
#include <regex>

namespace sw {

void add_file_to_storage(auto &&s, auto &&f) {
}
void add_transform_to_storage(auto &&s, auto &&f) {
    add_file_to_storage(s,f);
}

auto build_some_package(auto &s) {
    files_target tgt;
    tgt.name = "pkg1";
    tgt.source_dir = s.source_dir;
    tgt +=
        "src"_rdir,
        "src/main.cpp",
        "src/.*\\.cpp"_r,
        "src/.*\\.h"_rr
        ;
    return tgt;
}

namespace os {

struct windows {};

} // namespace os

namespace arch {

struct x86 {};
struct x64 {};
using amd64 = x64;
using x86_64 = x64;

struct arm64 {};
using aarch64 = arm64;

} // namespace os

auto build_some_package2(auto &s) {
    auto &tgt = s.template add<rule_target>();
    tgt.name = "pkg2";
    tgt +=
        "src"_rdir,
        "src/main.cpp",
        "src/.*\\.cpp"_r,
        "src/.*\\.h"_rr
        ;
    visit1(s.os, [&](os::windows &) {
        tgt += "advapi32.lib"_slib;
        tgt += "ole32.lib"_slib;
        tgt += "OleAut32.lib"_slib;
    });
    tgt += native_sources_rule{};
    //tgt += s.cl_rule;
    //tgt += s.link_rule;
    tgt += cl_exe_rule{};
    tgt += link_exe_rule{};
}

} // namespace sw

using namespace sw;

struct solution {
    abspath source_dir;
    abspath binary_dir;
    // config

    os::windows os;
    // arch
    // libtype
    // default compilers
    //

    // internal data
    std::vector<std::unique_ptr<target>> targets_;

    template <typename T, typename ... Args>
    T &add(Args && ... args) {
        auto &v = *targets_.emplace_back(std::make_unique<target>(T{FWD(args)...}));
        auto &t = std::get<T>(v);
        t.source_dir = source_dir;
        t.binary_dir = binary_dir;
        return t;
    }

    auto targets() {
        return targets_ | std::views::transform([](auto &&v) -> decltype(auto) { return *v; });
    }
    void build() {
        for (auto &&t : targets()) {
            visit(t, [&](auto &&v) {
                if constexpr (requires {v();}) {
                    v();
                }
            });
        }
    }
};

int main1() {
#if defined(_WIN32) && !defined(__MINGW32__)
    SetConsoleOutputCP(CP_UTF8);
#endif

    solution s {
        ".", ".sw4", /*{native_sources_rule{},
#ifdef _WIN32
                        cl_exe_rule{}, link_exe_rule{}
#else
                        gcc_compile_rule{}, gcc_link_rule{}
#endif
    }*/};
    auto tgt = build_some_package(s);
    build_some_package2(s);

    s.build();

	/*file_storage<physical_file_storage_single_file<basic_contents_hash>> fst{ {"single_file2.bin"} };
    fst += tgt;
	for (auto &&handle : fst) {
		handle.copy("myfile.txt");
	}*/
    return 0;
}

int main() {
    try {
        return main1();
    } catch (std::exception &e) {
        std::cerr << e.what();
    } catch (...) {
        std::cerr << "unknown exception";
    }
    return 1;
}
