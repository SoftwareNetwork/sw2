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

auto build_some_package2(auto &s) {
    rule_target tgt;
    tgt.name = "pkg2";
    tgt.source_dir = s.source_dir;
    tgt +=
        "src"_rdir,
        "src/main.cpp",
        "src/.*\\.cpp"_r,
        "src/.*\\.h"_rr
        ;
#ifdef _WIN32
    tgt.link_options.link_libraries.push_back("advapi32.lib");
    tgt.link_options.link_libraries.push_back("ole32.lib");
    tgt.link_options.link_libraries.push_back("OleAut32.lib");
#endif
    tgt += sources_rule{};
    tgt += s.cl_rule;
    tgt += s.link_rule;
    tgt();
    return tgt;
}

} // namespace sw

using namespace sw;

struct solution {
    path source_dir;
    path build_dir;
    // config

#ifdef _WIN32
    cl_exe_rule cl_rule{};
    link_exe_rule link_rule{};
#else
    gcc_compile_rule cl_rule{};
    gcc_link_rule link_rule{};
#endif
};

int main1() {
    solution s{".", ".sw"};
    auto tgt = build_some_package(s);
    auto tgt2 = build_some_package2(s);
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
