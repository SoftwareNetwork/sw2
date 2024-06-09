#pragma once

#include "command/command.h"

namespace sw {

struct path2 {
    using element = variant<string, string_view>;

    std::vector<element> elements;
};

struct rule {
    struct vertical_expander {
    };
    struct horizontal_expander {
    };
    using element = variant<string, string_view, path2, vertical_expander, horizontal_expander>;

    std::vector<element> arguments;

    void add(auto &&p) {
        if constexpr (requires { std::to_string(p); }) {
            add(std::to_string(p));
        } else if constexpr (requires { arguments.push_back(p); }) {
            arguments.push_back(p);
        } else {
            for (auto &&a : p) {
                add(a);
            }
        }
    }
    void add(const char *p) {
        arguments.push_back(string{p});
    }
    auto operator+=(auto &&arg) {
        add(arg);
        return appender{[&](auto &&v) {
            add(v);
        }};
    }

    std::vector<command> commands(auto &&env) const {
        std::vector<command> cmds;
        cmds.emplace_back(io_command{});
        for (auto &&a : arguments) {
            visit(a
                , [&](const vertical_expander &){}
                , [&](const horizontal_expander &){}
                , [&](const path2 &){}
                , [&](auto &&a) {
                    for (auto &&c : cmds) {
                        visit(c, [&](auto &&c) {
                            c += a;
                        });
                    }
                }
            );
        }
        for (auto &&c : cmds) {
            visit(c, [&](auto &&c) {
                visit(c.arguments.at(0), [&](const path &p) {
                        c.arguments[0] = env.resolve(p.string());
                    },
                    [&](const auto &p) {
                        c.arguments[0] = env.resolve(p);
                    });
            });
        }
        return cmds;
    }
};

} // namespace sw
