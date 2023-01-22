void build(solution &s) {
    auto &lib = s.add<native_library_target>("sw.lib");
    lib += "src/.*\\.h"_rdir;
    lib.public_ += "src"_idir;
    if (lib.is<os::windows>()) {
        lib += "advapi32.lib"_slib;
        lib += "ole32.lib"_slib;
        lib += "OleAut32.lib"_slib;
    }
    if (lib.is<os::macos>()) {
        // for now
        lib.public_ += "/usr/local/opt/fmt/include"_idir; // github ci
        lib.public_ += "/opt/homebrew/include"_idir; // brew
    }

    auto &exe = s.add<executable_target>("sw");
    exe += "src/sw/main.cpp";
    exe += lib;

    /*if (tgt.is<cpp_compiler::msvc>()) {
    }
    {
        //s.add<test>();
        //s.add<test_command>();
    }*/
}
