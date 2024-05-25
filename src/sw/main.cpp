// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2022 Egor Pugin <egor.pugin@gmail.com>

#include "runtime/command_line.h"
#include "runtime/main.h"
#include "sys/log.h"
#include "generator/common.h"
#include "builtin/default_settings.h"
#include "startup.h"
#include "sw_tool.h"

#include "rule.h"
#include "sw.h"
using namespace sw;

/*void sw1(auto &cl) {
    visit(
        cl.c,
        [&](auto &b) requires (false
            || std::same_as<std::decay_t<decltype(b)>, command_line_parser::build>
            || std::same_as<std::decay_t<decltype(b)>, command_line_parser::test>
            || std::same_as<std::decay_t<decltype(b)>, command_line_parser::generate>
            ) {
            auto s = make_solution();
            std::vector<entry_point> entry_points;
#ifdef SW1_BUILD
            entry_points = sw1_load_inputs();
#else
            throw std::runtime_error{"no entry function was specified"};
#endif

            {
                std::vector<build_settings> settings{default_build_settings()};
                swap_and_restore sr{s.dry_run, true};
                for (auto &&ep : entry_points) {
                    input_with_settings is;
                    is.ep = entry_point{ep.build, ep.source_dir};
                    is.settings.insert(settings.begin(), settings.end());
                    s.gather_entry_points(is);
                }
            }

            auto settings = make_settings(b);
            for (auto &&ep : entry_points) {
                input_with_settings is;
                is.ep = entry_point{ep.build, ep.source_dir};
                is.settings.insert(settings.begin(), settings.end());
                s.add_input(is);
            }
            if constexpr (std::same_as<std::decay_t<decltype(b)>, command_line_parser::build>) {
                s.build(cl);
            }
            if constexpr (std::same_as<std::decay_t<decltype(b)>, command_line_parser::test>) {
                s.test(cl);

                if (b.format.value != "junit") {
                    SW_UNIMPLEMENTED;
                }
                // https://llg.cubic.org/docs/junit/
                xml_emitter e;
                {
                    struct data {
                        int tests{};
                        int failures{};
                        int errors{};
                        int skipped{};
                        io_command::clock::duration time{};
                        // time

                        void operator+=(const data &d) {
                            tests += d.tests;
                            failures += d.failures;
                            errors += d.errors;
                            skipped += d.skipped;
                            time += d.time;
                        }
                    };
                    auto format_time = [&](auto &&t) {
                        auto f = std::chrono::duration_cast<std::chrono::duration<float>>(t).count();
                        return format("{}", f);
                    };
                    auto set_attrs = [&](auto &&o, auto &&d) {
                        o["tests"] = std::to_string(d.tests);
                        o["skipped"] = std::to_string(d.skipped);
                        o["errors"] = std::to_string(d.errors);
                        o["failures"] = std::to_string(d.failures);
                        o["time"] = format_time(d.time);
                    };
                    data dtestsuites;
                    auto testsuites = e.tag("testsuites");
                    for (auto &&[id, t] : s.targets) {
                        visit(t, [&](auto &&vp) {
                            auto &v = *vp;
                            if constexpr (requires { v.tests; }) {
                                if (v.tests.empty()) {
                                    return;
                                }
                                data dtestsuite;
                                auto testsuite = testsuites.tag("testsuite");
                                testsuite["name"] = (string)v.name;
                                testsuite["package"] = (string)v.name;
                                testsuite["config"] = std::to_string(v.bs.hash());
                                for (auto &&t : v.tests) {
                                    ++dtestsuite.tests;
                                    auto tc = testsuite.tag("testcase");
                                    visit(t, [&](auto &&c) {
                                        auto p = c.name_.rfind('[');
                                        if (p == -1) {
                                            tc["name"] = c.name_;
                                        } else {
                                            ++p;
                                            tc["name"] = c.name_.substr(p, c.name_.rfind(']') - p);
                                        }
                                        bool time_not_set = c.start == decltype(c.start){};
                                        if (time_not_set && c.processed) {
                                            ++dtestsuite.skipped;
                                            tc.tag("skipped");
                                            return;
                                        }
                                        auto testdir = std::get<path>(c.err.s).parent_path();
                                        if (!c.exit_code || !c.processed) {
                                            auto e = tc.tag("error");
                                            if (c.processed) {
                                                e["message"] = "test was not executed";
                                            } else {
                                                e["message"] = "test dependencies failed";
                                            }
                                            ++dtestsuite.errors;
                                        } else if (*c.exit_code) {
                                            auto e = tc.tag("failure");
                                            e["message"] = c.get_error_message();
                                            tc["time"] = format_time(c.end - c.start);
                                            ++dtestsuite.failures;
                                            dtestsuite.time += c.end - c.start;
                                            write_file(testdir / "exit_code.txt", format("{}", *c.exit_code));
                                            write_file(testdir / "time.txt", format_time(c.end - c.start));
                                        } else {
                                            tc["time"] = format_time(c.end - c.start);
                                            dtestsuite.time += c.end - c.start;
                                            write_file(testdir / "exit_code.txt", format("{}", *c.exit_code));
                                            write_file(testdir / "time.txt", format_time(c.end - c.start));
                                        }
                                    });
                                    // sw1 has "config" attribute here
                                }
                                set_attrs(testsuite, dtestsuite);
                                dtestsuites += dtestsuite;
                            }
                        });
                    }
                    set_attrs(testsuites, dtestsuites);

                    log_info(R"(
Test results:
TOTAL:   {}
PASSED:  {}
FAILED:  {}
SKIPPED: {})",
                             dtestsuites.tests, dtestsuites.tests - (dtestsuites.failures + dtestsuites.errors),
                             dtestsuites.failures + dtestsuites.errors, dtestsuites.skipped);
                    // List of skipped tests:
                }
                auto resfn = s.work_dir / "test" / "results.xml";
                write_file(resfn, e.s);
            }
            if constexpr (std::same_as<std::decay_t<decltype(b)>, command_line_parser::generate>) {
                auto ce = s.make_command_executor();
                ce.prepare(cl, s);
                if (!b.generator) {
#ifdef _WIN32
                    b.generator.value = "vs";
#endif
                }
                if (!b.generator) {
                    throw std::runtime_error{"specify generator with -g"};
                }
                auto g = [&]<typename ... Types>(variant<Types...>**){
                    generators g;
                    if (!((Types::name == *b.generator.value && (g = Types{}, true)) || ... || false)) {
                        throw std::runtime_error{"unknown generator: "s + *b.generator.value};
                    }
                    return g;
                }((generators**)nullptr);
                visit(g, [&](auto &&g) {
                    g.generate(s, ce);
                });
            }
        },
        [](auto &&) {
            SW_UNIMPLEMENTED;
        });
}*/

auto clrule(auto &&input_file) {
    rule r;
    r += "cl", "-c", input_file;
    return r;
}

int main1(int argc, char *argv[]) {
    startup_data sd{argc,argv};
    return sd.run();
}
/*void main2(int argc, char *argv[]) {
    command_line_parser cl{argc, argv};
    visit_any(
        cl.c,
        [&](auto &b) requires(false || std::same_as<std::decay_t<decltype(b)>, command_line_parser::run> ||
                              std::same_as<std::decay_t<decltype(b)>, command_line_parser::exec>) {
                         auto sln = make_solution();
                         sln.binary_dir = temp_sw_directory_path() / ".sw";

                         auto it = std::ranges::find(*b.arguments.value, "--"sv);
                         if (it == b.arguments.value->end()) {
                             it = b.arguments.value->begin() + 1;
                         }
                         using ttype = executable;
                         auto tname = path{*b.arguments.value->begin()}.stem().string();
                         if (tname == "sw"sv) {
                             tname = "sw1";
                         }

                         auto orig = fs::absolute(*b.arguments.value->begin());
                         auto s = read_file(orig);
                         if (s.starts_with("#!/bin/sw")) {
                             b.remove_shebang.value = true;
                         }
                         auto first_line = s.substr(0, s.find('\n'));
                         auto unix = fs::exists("/bin/sh");
                        if (b.remove_shebang) {
                            auto fn = temp_sw_directory_path() / "run" / std::to_string(std::hash<path>{}(orig)) += ".cpp";
                            // we are trying to detect env, by default shebang is in .sh scripts
                            string out;
                            // 1. pch?
                            // 2. add useful functions library... like read_file write_file etc.
                            if (unix) {
                                out += R"(#include <bits/stdc++.h>
)";
                            } else {
                                out += R"(
#define _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS 1
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <__msvc_all_public_headers.hpp>

#include <windows.h>
#include <objbase.h>
)";
                            }
                            out += R"(
using namespace std;
using namespace std::literals;
namespace fs = std::filesystem;
using namespace fs;
)";
                            out += format("#line 2 \"{}\"\n{}", *b.arguments.value->begin(), s.substr(s.find('\n')));
                            write_file_if_different(fn, out);
                            *b.arguments.value->begin() = fn.string();
                        }
                         auto ep = [&](solution &s) {
                             auto &t = s.add<ttype>(tname);
                             for (auto &&f : std::ranges::subrange(b.arguments.value->begin(), it)) {
                                 t += f;
                             }
                             if (!unix && b.remove_shebang) {
                                 // add deps here!
                                 //command_line_parser::run_common r;
                                 //command_line_parser::parse1(r, );
                                 t += "sw.lib"_dep;
                             }
                         };
                         input_with_settings is;
                         is.ep.build = [](auto &&s){
                             //self_build::sw_build b;
                             //b.build(s);
                         };
                         is.ep.source_dir = get_sw_dir();
                         auto settings = make_settings(b);
                         is.settings.insert(settings.begin(), settings.end());
                         sln.add_input(is);
                         is.ep = entry_point{ep, fs::current_path()};
                         sln.add_input(is);
                         sln.build(cl);

                         auto &&t = sln.targets.find_first<ttype>(tname);
                         raw_command c;
                         c.working_directory = fs::current_path();
                         c += t.executable;
                        for (auto &&o : std::ranges::subrange(it, b.arguments.value->end())) {
                            c += o;
                        }
                         if constexpr (std::same_as<std::decay_t<decltype(b)>, command_line_parser::exec>) {
                             c.exec = true;
                         } else {
                             auto &r = std::get<command_line_parser::run>(cl.c);
                             if (r.exec) {
                                 c.exec = true;
                             }
                         }
                         try {
                             return c.run();
                         } catch (std::exception &) {
                             // supress exception message
                             return !c.exit_code ? 1 : *c.exit_code;
                         }
                         return 0;
                     },
                     [&](auto &)
        {
            return 0;
        });
}*/
