#pragma once

#include "helpers.h"

int main1(std::span<string_view>);

int main(int argc, char *argv[]) {
    try {
        std::vector<string_view> args((const char **)argv, (const char **)argv + argc);
        return main1(args);
    } catch (std::exception &e) {
        std::cerr << e.what() << "\n";
    } catch (...) {
        std::cerr << "unknown exception" << "\n";
    }
    return 1;
}
