// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2022 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include "command.h"

namespace sw {

struct command_executor {
    static void create_output_dirs(auto &&commands) {
        std::unordered_set<path> dirs;
        for (auto &&c : commands) {
            visit(*c, [&](auto &&c) {
                if (!c.working_directory.empty()) {
                    dirs.insert(c.working_directory);
                }
                for (auto &&o : c.outputs) {
                    if (auto p = o.parent_path(); !p.empty()) {
                        dirs.insert(p);
                    }
                }
            });
        }
        for (auto &&d : dirs) {
            fs::create_directories(d);
        }
    }
    static void make_dependencies(auto &&commands) {
        std::map<path, void *> cmds;
        for (auto &&c : commands) {
            visit(*c, [&](auto &&c1) {
                for (auto &&f : c1.outputs) {
                    auto [_, inserted] = cmds.emplace(f, c);
                    if (!inserted) {
                        throw std::runtime_error{"more than one command produces: "s + f.string()};
                    }
                }
            });
        }
        for (auto &&c : commands) {
            visit(*c, [&](auto &&c1) {
                for (auto &&f : c1.inputs) {
                    if (auto i = cmds.find(f); i != cmds.end()) {
                        c1.dependencies.insert(i->second);
                        visit(*(command *)i->second, [&](auto &&d1) {
                            d1.dependents.insert(c);
                        });
                    }
                }
                c1.n_pending_dependencies = c1.dependencies.size();
            });
        }
    }

    static void check_dag1(auto &&c) {
        if (c.dagstatus == io_command::dag_status::no_circle) {
            return;
        }
        if (c.dagstatus == io_command::dag_status::visited) {
            throw std::runtime_error{"circular dependency detected"};
        }
        c.dagstatus = io_command::dag_status::visited;
        for (auto &&d : c.dependencies) {
            visit(*(command *)d, [&](auto &&d1) {
                check_dag1(d1);
            });
        }
        c.dagstatus = io_command::dag_status::no_circle;
    }
    static void check_dag(auto &&commands) {
        // rewrite into non recursive?
        for (auto &&c : commands) {
            visit(*c, [&](auto &&c) {
                check_dag1(c);
            });
        }
    }

    struct pending_commands {
        std::deque<command *> commands;

        void push_back(command *cmd) {
            commands.push_back(cmd);
        }
        bool empty() const {
            return commands.empty() || std::ranges::all_of(commands, [](auto &&cmd) {
                       return visit(*cmd, [&](auto &&c) {
                           return c.simultaneous_jobs && *c.simultaneous_jobs == 0;
                       });
                })
                ;
        }
        command *next() {
            auto it = std::ranges::find_if(commands, [](auto &&cmd) {
                return visit(*cmd, [&](auto &&c) {
                    return !c.simultaneous_jobs || *c.simultaneous_jobs != 0;
                });
            });
            auto c = *it;
            commands.erase(it);
            return c;
        }
    };

    pending_commands pending_commands_;
    int running_commands{0};
    size_t maximum_running_commands{std::thread::hardware_concurrency()};
    executor *ex_external{nullptr};
    std::vector<command> owned_commands;
    std::vector<command*> external_commands;
    // this number differs with external_commands.size() because we have:
    // 1. pipes a | b | ...
    // 2. command sequences a; b; c; ...
    // 3. OR or AND chains: a || b || ... ; a && b && ...
    int number_of_commands{};
    int command_id{};
    std::vector<command*> errors;
    int ignore_errors{0};
    bool explain_outdated{};

    command_executor() {
        init();
    }
    void init() {
#ifdef _WIN32
        // always create top level job and associate self with it
        win32::default_job_object();
#endif
    }

    auto &get_executor() {
        if (!ex_external) {
            throw std::logic_error{"no executor set"};
        }
        return *ex_external;
    }
    bool is_stopped() const {
        return ignore_errors < errors.size();
    }
    void run_next_raw(auto &&cl, auto &&sln, auto &&cmd, auto &&c) {
        if (c.is_pipe_child()) {
            return;
        }
        ++command_id;
        auto run_dependents = [&]() {
            for (auto &&d : c.dependents) {
                visit(*(command *)d, [&](auto &&d1) {
                    if (!--d1.n_pending_dependencies) {
                        pending_commands_.push_back((command *)d);
                    }
                });
            }
        };
        c.processed = true;
        if (!c.outdated(explain_outdated)) {
            return run_dependents();
        }
        log_info("[{}/{}] {}", command_id, number_of_commands, c.name());
        log_trace(c.print());
        try {
            ++running_commands;
            if (c.simultaneous_jobs) {
                --(*c.simultaneous_jobs);
            }

            // use GetProcessTimes or similar for time
            // or get times directly from OS
            c.start = std::decay_t<decltype(c)>::clock::now();

            c.run(get_executor(), [&, run_dependents, cmd]() {
                c.end = std::decay_t<decltype(c)>::clock::now();

                if (c.simultaneous_jobs) {
                    ++(*c.simultaneous_jobs);
                }
                --running_commands;

                if (cl.save_executed_commands || cl.save_failed_commands && !c.ok()) {
                    c.save(get_saved_commands_dir(sln));
                }

                if (!c.ok()) {
                    errors.push_back(cmd);
                } else {
                    if constexpr (requires { c.process_deps(); }) {
                        c.process_deps();
                    }
                    if (c.cs) {
                        // pipe commands are not added yet - and they must be added when they are finished
                        c.cs->add(c);
                    }
                    run_dependents();
                }
                run_next(cl, sln);
            });
            return;
        } catch (std::exception &e) {
            c.out_text = e.what();
        }
        // error path, prevent exception recursion
        if (c.is_pipe_child()) {
            c.terminate_chain();
            return;
        }
        if (c.is_pipe_leader()) {
            c.terminate_chain();
        }
        if (c.simultaneous_jobs) {
            ++(*c.simultaneous_jobs);
        }
        --running_commands;
        errors.push_back(cmd);
        if (cl.save_executed_commands || cl.save_failed_commands) {
            // c.save(get_saved_commands_dir(sln));//save not started commands?
        }
        run_next(cl, sln);
    }
    void run_next(auto &&cl, auto &&sln) {
        while (running_commands < maximum_running_commands && !pending_commands_.empty() && !is_stopped()) {
            auto cmd = pending_commands_.next();
            visit(*cmd, [&](auto &&c) {
                run_next_raw(cl, sln, cmd, c);
            });
        }
    }
    void run(auto &&cl, auto &&sln) {
        prepare(cl, sln);

        // initial set of commands
        for (auto &&c : external_commands) {
            visit(*c, [&](auto &&c1) {
                if (c1.dependencies.empty()) {
                    pending_commands_.push_back(c);
                }
            });
        }
        run_next(cl, sln);
        get_executor().run();
    }
    void prepare(auto &&cl, auto &&sln) {
        prepare1(cl, sln);
        create_output_dirs(external_commands);
        make_dependencies(external_commands);
        check_dag(external_commands);
    }
    void prepare1(auto &&cl, auto &&sln) {
        visit_any(
            cl.c,
            [&](auto &b) requires requires {b.explain_outdated;} {
            explain_outdated = b.explain_outdated.value;
        });
        if (cl.jobs) {
            maximum_running_commands = cl.jobs;
        }
        visit(cl.c, [&](auto &&c) {
            if constexpr (requires {c.ignore_errors;}) {
                if (c.ignore_errors) {
                    ignore_errors = c.ignore_errors;
                }
            }
        });
        for (auto &&c : external_commands) {
            visit(*c, [&](auto &&c) {
                if (cl.rebuild_all) {
                    c.always = true;
                }
                if (!c.is_pipe_child()) {
                    ++number_of_commands;
                }
                if (auto p = std::get_if<path>(&c.in.s)) {
                    c.inputs.insert(*p);
                }
                if (auto p = std::get_if<path>(&c.out.s)) {
                    c.outputs.insert(*p);
                }
                if (auto p = std::get_if<path>(&c.err.s)) {
                    c.outputs.insert(*p);
                }
            });
        }
        for (auto &&c : external_commands) {
            visit(*c, [&](auto &&c) {
                if (c.is_pipe_leader()) {
                    c.pipe_iterate([&](auto &&ch) {
                        c.inputs.insert(ch.io->inputs.begin(), ch.io->inputs.end());
                        //c.outputs.insert(ch.io->outputs.begin(), ch.io->outputs.end());
                    });
                }
            });
        }
    }
    void check_errors() {
        if (errors.empty()) {
            return;
        }
        string t;
        for (auto &&cmd : errors) {
            visit(*cmd, [&](auto &&c) {
                t += c.get_error_message() + "\n";
            });
        }
        t += "Total errors: " + std::to_string(errors.size());
        throw std::runtime_error{t};
    }
    path get_saved_commands_dir(auto &&sln) {
        return sln.work_dir / "rsp";
    }

    void operator+=(command &c) {
        external_commands.push_back(&c);
    }
    void operator+=(std::vector<command> &commands) {
        for (auto &&c : commands) {
            external_commands.push_back(&c);
        }
    }
    void operator+=(std::vector<command> &&commands) {
        owned_commands.reserve(owned_commands.size() + commands.size());
        for (auto &&c : commands) {
            auto &&p = owned_commands.emplace_back(std::move(c));
            external_commands.push_back(&p);
        }
    }
};

} // namespace sw
