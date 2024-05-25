#pragma once

#include "../helpers/common.h"
#include "../sys/log.h"

int main1(int argc, char *argv[]);

int main(int argc, char *argv[]) {
    try {
        return main1(argc, argv);
    } catch (std::exception &e) {
        sw::log_error("{}", e.what());
    } catch (...) {
        sw::log_error("unknown exception");
    }
    return 1;
}
