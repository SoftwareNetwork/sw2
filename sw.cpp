void build(Solution &s) {
    auto &t = s.addExecutable("sw");
    {
        t.SwDefinitions = true;
        t.PackageDefinitions = true;
        t += cpp23;
        t += "src/.*"_rr;
        t.Public += "src"_idir;
        t += "src/sw"_idir;
        if (t.getBuildSettings().TargetOS.Type == OSType::Windows) {
            t += "ole32.lib"_slib;
            t += "OleAut32.lib"_slib;
            t += "advapi32.lib"_slib;
        }
        if (t.getBuildSettings().TargetOS.Type == OSType::Mingw) {
            t += "org.sw.demo.mingw.w64.headers.windows.ole32-0.0.1"_dep;
            t += "org.sw.demo.mingw.w64.headers.windows.oleaut32-0.0.1"_dep;
            t += "org.sw.demo.mingw.w64.headers.windows.advapi32-0.0.1"_dep;
        }
        if (t.getCompilerType() == CompilerType::MSVC) {
            t.CompileOptions.push_back("/bigobj");
        }
    }
}

// win: cl -nologo -std:c++latest -EHsc src/*.cpp -link -OUT:sw.exe
// lin: g++ -std=c++2b src/*.cpp -o sw
// mac: g++-12 -std=c++2b src/*.cpp -o sw
