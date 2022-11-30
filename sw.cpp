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

// win: cl -nologo -std:c++latest -EHsc src/*.cpp -link -OUT:sw.exe
// lin: g++ -std=c++2b src/*.cpp -o sw
// mac: g++-12 -std=c++2b src/*.cpp -o sw
