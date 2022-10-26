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
    std::vector<target_ptr> targets_;
    std::vector<rule> rules;

    solution() {
        rules.push_back(cl_exe_rule{});
        rules.push_back(link_exe_rule{});
    }

    template <typename T, typename... Args>
    T &add(Args &&...args) {
        auto ptr = std::make_unique<T>(*this, FWD(args)...);
        auto &t = *ptr;
        targets_.emplace_back(std::move(ptr));
        return t;
    }

    auto &targets() {
        return targets_;
        //| std::views::transform([](auto &&v) -> decltype(auto) {
                   //return *v;
               //});
    }
    void prepare() {
        for (auto &&t : targets()) {
            visit(t, [&](auto &&vp) {
                auto &v = *vp;
                if constexpr (std::derived_from<std::decay_t<decltype(v)>, rule_target>) {
                    for (auto &&r : rules) {
                        v += r;
                    }
                }
                if constexpr (requires { v.prepare(); }) {
                    v.prepare();
                }
            });
        }
    }
    void build() {
        executor ex;
        build(ex);
    }
    void build(auto &&ex) {
        prepare();

        command_executor ce{ex};
        for (auto &&t : targets()) {
            visit(t, [&](auto &&vp) {
                auto &v = *vp;
                if constexpr (requires { v.commands; }) {
                    ce += v.commands;
                }
            });
        }
        ce.run();
    }
};

} // namespace sw
