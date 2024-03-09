struct zlib {
    void build(solution &s) {
        auto &tgt = s.add<native_library_target>("zlib");
        tgt += ".*\\.[hc]"_r;
        tgt.public_ += "."_idir;
        if (tgt.is<library_type::shared>()) {
            tgt += "ZLIB_DLL"_def;
        }
    }
};
