#pragma once

#include "system.h"

namespace sw {

// sw_context?
// sw_command_runner?
struct sw_tool {
    path config_dir;
    path storage_dir;
    path temp_dir;

    system sys;

    builtin_repository builtin_repo;
    repository repo;

    sw_tool() {
        config_dir = get_home_directory() / ".sw2";
#ifndef BUILTIN_STORAGE_DIR
#endif
        auto storfn = config_dir / "storage_dir";
        if (!fs::exists(storfn)) {
            write_file(storfn, (const char *)(config_dir / "storage").u8string().c_str());
        }
        storage_dir = (const char8_t *)read_file(config_dir / "storage_dir").c_str();
        temp_dir = temp_sw_directory_path();
        if (!fs::exists(temp_dir)) {
            fs::create_directories(temp_dir);
            std::error_code ec;
            fs::permissions(temp_dir, fs::perms::all, ec);
        }
    }
    void init() {
        builtin_repo.init(*this);
        repo.init(*this);
    }
    int run_command_line() {
        return 0;
    }

    path pkg_root(auto &&name, auto &&version) const {
        return storage_dir / "pkg" / name / (string)version;
    }
    path mirror_fn(auto &&name, auto &&version) const {
        auto ext = ".zip";
        return storage_dir / "mirror" / (name + "_" + (string)version + ext);
    }

    bool local_mode() const {
        // do we use internet or not
        return false;
    }
};

} // namespace sw
