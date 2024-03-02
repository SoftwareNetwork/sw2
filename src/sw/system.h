#pragma once

namespace sw {

// represents host system
struct system {
    // arch
    // installed packages
    // kernel is also a package

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
