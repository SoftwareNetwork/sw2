// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2022 Egor Pugin <egor.pugin@gmail.com>

#include "sw.h"
#include "command_line.h"
#include "main.h"

void lower_drive_letter(string &s) {
    if (!s.empty()) {
        s[0] = tolower(s[0]);
    }
}
auto normalize_path(const path &p) {
    auto fn = p.string();
    std::replace(fn.begin(), fn.end(), '\\', '/');
    return fn;
}

struct cpp_emitter {
    struct ns {
    };

    string s;
    int indent{};

    void include(const path &p) {
        auto fn = normalize_path(p);
        lower_drive_letter(fn);
        s += "#include \"" + fn + "\"\n";
    }
};

int main1(int argc, char *argv[]) {
    command_line_parser cl{argc, argv};

    if (cl.working_directory) {
        fs::current_path(cl.working_directory);
    }
    visit_any(cl.c, [](command_line_parser::build &b) {
        solution s;
        auto fn = s.binary_dir / "cfg" / "src" / "main.cpp";
        fs::create_directories(fn.parent_path());
        cpp_emitter e;
        e.include(path{std::source_location::current().file_name()}.parent_path() / "sw.h");
        visit_any(b.i, [&](specification_file_input &i) {
            e.include(fs::absolute(i.fn));
        });
        e.include(path{std::source_location::current().file_name()}.parent_path() / "main.cpp");
        write_file_if_different(fn, e.s);

        s.source_dir = s.binary_dir / "cfg";
        s.add_input(entry_point{&self_build::build});
        s.build();

        auto &&t = s.targets.find_first<executable>("sw");
        int a = 5;
        a++;
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
