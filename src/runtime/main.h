#pragma once

#include "../helpers/common.h"
#include "../sys/log.h"

int main1(int argc, char *argv[]);

int main(int argc, char *argv[]) {
    try {
        return main1(argc, argv);
    } catch (std::exception &e) {
        log_error(e.what());
    } catch (...) {
        log_error("unknown exception");
    }
    return 1;
}
