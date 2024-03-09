#pragma once

#include "helpers/common.h"
#include "runtime/command_line.h"
#include "sys/log.h"

namespace sw {

struct startup_data {
    int argc;
    char **argv;
    vector<string> args;
    command_line_parser cl;

    int run() {
        // setConsoleColorProcessing();
        args.assign(argv, argv + argc);
        // prepare args - replace %20 from uri invocation
        cl.parse(args);

        // set wdir
        auto this_path = fs::current_path();
        if (cl.working_directory) {
            fs::current_path(cl.working_directory);
        }

        init_logger();

        if (cl.version) {
            log_trace("sw2");
            return 0;
        }
        if (cl.sleep) {
            log_trace("sleep started");
            std::this_thread::sleep_for(std::chrono::seconds(cl.sleep));
            log_trace("sleep completed");
        }

        return sw_main();
    }
    int sw_main() {
        if (cl.sw1) {
            if (cl.int3) {
                debug_break_if_not_attached();
            }
            // sw1(cl);
            return 1;
        }
#ifdef SW1_BUILD
        return 0;
#endif
        cl.rebuild_all = false; // not for config/run builds
    }
    void init_logger() {
        if (cl.trace) {
            log_settings.log_level = std::max(log_settings.log_level, 6);
        }
        if (cl.verbose) {
            log_settings.log_level = std::max(log_settings.log_level, 4);
        }
        if (cl.log_level) {
            log_settings.log_level = std::max<int>(log_settings.log_level, cl.log_level);
        }
    }
};

}
