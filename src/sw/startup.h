#pragma once

#include "helpers/common.h"
#include "runtime/command_line.h"
#include "sys/log.h"
#include "sw_tool.h"

namespace sw {

struct startup_data {
    int argc;
    char **argv;
    vector<string> args;
    command_line_parser cl;

    void run() {
        // setConsoleColorProcessing();
        args.assign(argv, argv + argc);
        // prepare args - replace %20 from uri invocation
        cl.parse(args);

        // set wdir
        auto this_path = fs::current_path();
        if (cl.working_directory) {
            fs::current_path(cl.working_directory.value->stdpath());
        }

        init_logger();

        if (cl.version) {
            log_trace("sw2");
            return;
        }
        if (cl.sleep) {
            log_trace("sleep started");
            std::this_thread::sleep_for(std::chrono::seconds(cl.sleep));
            log_trace("sleep completed");
        }

        sw_main();
    }
    void sw_main() {
        sw_tool t;
        t.run_command_line(cl);
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
