#pragma once

#include "rule_target.h"
#include "os.h"

namespace sw {

struct solution {
    abspath source_dir{"."};
    abspath binary_dir{".sw4"};
    // config

    os::windows os;
    // arch
    // libtype
    // default compilers
    //

    // internal data
    std::vector<std::unique_ptr<target>> targets_;
    std::vector<rule> rules;

    solution() {
        rules.push_back(cl_exe_rule{});
        rules.push_back(link_exe_rule{});
    }

    template <typename T, typename... Args>
    T &add(Args &&...args) {
        auto &v = *targets_.emplace_back(std::make_unique<target>(T{FWD(args)...}));
        auto &t = std::get<T>(v);
        t.source_dir = source_dir;
        t.binary_dir = binary_dir;
        return t;
    }

    auto targets() {
        return targets_ | std::views::transform([](auto &&v) -> decltype(auto) {
                   return *v;
               });
    }
    void build() {
        executor ex;
        build(ex);
    }
    void build(auto &&ex) {
        for (auto &&t : targets()) {
            visit(t, [&](auto &&v) {
                if constexpr (std::derived_from<std::decay_t<decltype(v)>, rule_target>) {
                    for (auto &&r : rules) {
                        v += r;
                    }
                }
                if constexpr (requires { v.build(); }) {
                    v.build();
                }
            });
        }
    }
};

} // namespace sw
