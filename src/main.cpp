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
#include "command_line.h"
#include "main.h"

#include <map>
#include <regex>

namespace sw {

void add_file_to_storage(auto &&s, auto &&f) {
}
void add_transform_to_storage(auto &&s, auto &&f) {
    add_file_to_storage(s,f);
}

} // namespace sw

namespace sw::self_build {
#include "../sw.h"
} // namespace sw::self_build

using namespace sw;

int main1(int argc, char *argv[]) {
    command_line_parser cl{argc, argv};

    if (cl.working_directory) {
        fs::current_path(cl.working_directory);
    }
    visit_any(cl.c, [](command_line_parser::build &b) {
        solution s;
        auto fn = s.binary_dir / "cfg" / "main.cpp";
        fs::create_directories(fn.parent_path());
        string t;
        write_file(fn, t);
    });
    return 0;

    solution s;
    //s.add_input(source_code_input{&build_some_package});
    s.add_input(entry_point{&self_build::build});
    auto add = [&](auto f) {
        input_with_settings is{entry_point{f}};
        auto dbs = default_build_settings();
        auto set1 = [&]() {
            dbs.arch = arch::x64{};
            is.settings.insert(dbs);
            dbs.arch = arch::x86{};
            is.settings.insert(dbs);
            dbs.arch = arch::arm64{};
            is.settings.insert(dbs);
            dbs.arch = arch::arm{};
            is.settings.insert(dbs);
        };
        auto set2 = [&] {
            dbs.library_type = library_type::shared{};
            set1();
            dbs.library_type = library_type::static_{};
            set1();
        };
        auto set3 = [&] {
            dbs.build_type = build_type::debug{};
            set2();
            dbs.build_type = build_type::release{};
            set2();
        };
        dbs.c_static_runtime = true;
        set3();
        dbs.c_static_runtime = false;
        set3();
        s.add_input(std::move(is));
    };
    //add(&self_build);
    s.build();

	/*file_storage<physical_file_storage_single_file<basic_contents_hash>> fst{ {"single_file2.bin"} };
    fst += tgt;
	for (auto &&handle : fst) {
		handle.copy("myfile.txt");
	}*/
    return 0;
}
