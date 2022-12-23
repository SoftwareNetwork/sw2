#pragma once

#include "helpers.h"

int main1(int argc, char *argv[]);

int main(int argc, char *argv[]) {
    try {
        return main1(argc, argv);
    } catch (std::exception &e) {
        std::cerr << e.what();
    } catch (...) {
        std::cerr << "unknown exception";
    }
    return 1;
}
