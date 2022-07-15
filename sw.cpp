void build(Solution &s) {
    auto &t = s.addExecutable("sw");
    {
        t.SwDefinitions = true;
        t.PackageDefinitions = true;
        t += cpp23;
        t += "src/.*"_rr;
    }
}
