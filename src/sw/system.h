#pragma once

#include "sys/arch.h"
#include "package.h"
#include "setting.h"
#include "repository.h"

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
};

}
