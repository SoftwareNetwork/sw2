void build(Solution &s) {
    auto &t = s.addExecutable("sw");
    {
        t.SwDefinitions = true;
        t.PackageDefinitions = true;
        t += cpp23;
        t += "src/.*"_rr;
        if (t.getBuildSettings().TargetOS.Type == OSType::Windows) {
            t += "ole32.lib"_slib;
            t += "OleAut32.lib"_slib;
            t += "advapi32.lib"_slib;
        }
    }
}

// cl -nologo -std:c++latest -EHsc *.cpp advapi32.lib ole32.lib OleAut32.lib -link -OUT:sw.exe
