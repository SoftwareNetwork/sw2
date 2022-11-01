// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2022 Egor Pugin <egor.pugin@gmail.com>

#include "command.h"
#include "storage.h"
#include "vs_instance_helpers.h"
#include "package.h"
#include "detect.h"
#include "rule_target.h"
#include "os.h"
#include "solution.h"

#include <map>
#include <regex>

namespace sw {

void add_file_to_storage(auto &&s, auto &&f) {
}
void add_transform_to_storage(auto &&s, auto &&f) {
    add_file_to_storage(s,f);
}

} // namespace sw

using namespace sw;

auto build_some_package(solution &s) {
    files_target tgt{package_id{"pkg1"}};
    tgt.source_dir = s.source_dir;
    tgt +=
        "src"_rdir,
        "src/main.cpp",
        "src/.*\\.cpp"_r,
        "src/.*\\.h"_rr
        ;
    return tgt;
}

auto self_build(solution &s) {
    auto &tgt = s.add<native_target>(package_id{"pkg2"});
    tgt +=
        "src"_rdir,
        "src/main.cpp",
        "src/.*\\.cpp"_r,
        "src/.*\\.h"_rr
        ;
    if (tgt.is<os::windows>()) {
        tgt += "advapi32.lib"_slib;
        tgt += "ole32.lib"_slib;
        tgt += "OleAut32.lib"_slib;
    }
}

int main1() {
#if defined(_WIN32) && !defined(__MINGW32__)
    SetConsoleOutputCP(CP_UTF8);
#endif

    solution s;
    s.add_input(source_code_input{&build_some_package});
    s.add_input(source_code_input{&self_build});
    input_with_settings is{source_code_input{&self_build}};
    auto dbs = build_settings::default_build_settings();
    dbs.arch = arch::x86{};
    //is.settings.push_back(dbs);
    is.settings.insert(dbs);
    s.add_input(is);
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
