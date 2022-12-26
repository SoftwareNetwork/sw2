void build(solution &s) {
    auto &tgt = s.add<executable_target>("sw");
    tgt += "src"_rdir;
    if (tgt.is<os::windows>()) {
        tgt += "advapi32.lib"_slib;
        tgt += "ole32.lib"_slib;
        tgt += "OleAut32.lib"_slib;
    }
    {
        //s.add<test>();
        //s.add<test_command>();
    }
}
