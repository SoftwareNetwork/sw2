#pragma once

#include "sys/arch.h"
#include "package.h"
#include "setting.h"
#include "helpers/common.h"
// #include "repository.h"

struct cl_exe_rule1;
struct lib_exe_rule1;
struct link_exe_rule1;

using rule_list = sw::types<
    cl_exe_rule1,
    lib_exe_rule1,
    link_exe_rule1
>;

namespace sw {

struct settings__ {
    // arch
    // any other
    // per packages
    // recursive
};

// represents host system
// setting from template?
struct system {
    // installed packages
    // kernel is also a package

    path binary_dir{".sw"};
    arch_type arch{current_arch()};
    package_map<std::map<settings, rule_list::variant_type>> packages;

    system() {
    }

    bool is_sw_controlled() const {
#ifdef __linux__
        // check /.sw_controlled or init (1) pid - if it is ours
        return false;
#else
        return false;
#endif
    }

    /*auto make_solution() {
        solution s{*this, binary_dir, default_host_settings()};
        return s;
    }*/
};

}
