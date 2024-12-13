#pragma once

#include "runtime/command_line.h"
#include "builtin/default_settings.h"
#include "repository.h"
#include "solution.h"
#include "system.h"
#include "build.h"

namespace sw {

struct config_emitter {
    cpp_emitter2 e;
    //cpp_emitter2::stru cfg;

    config_emitter() {
        //cfg = std::move(e.struct_("config"));
    }
    ~config_emitter() {
    }

    void write(auto &&cfgdir) {
        auto &&text = e.text();
        auto h = digest<crypto::sha3<256>>(text);
        auto fn = cfgdir / h += ".h"s;
        if (!fs::exists(fn)) {
            write_file(fn, text);
        }
    }
};

// sw_context?
// sw_command_runner?
struct sw_tool {
    path config_dir;
    path storage_dir;
    path temp_dir;

    system sys;

    builtin_repository builtin_repo;
    //repository repo;

    sw_tool() {
        config_dir = get_home_directory() / ".sw2";
#ifndef BUILTIN_STORAGE_DIR
#endif
        auto storfn = config_dir / "storage_dir";
        if (!fs::exists(storfn)) {
            write_file(storfn, (config_dir / "storage").string());
        }
        storage_dir = read_file(config_dir / "storage_dir");
        temp_dir = temp_sw_directory_path();
        if (!fs::exists(temp_dir)) {
            fs::create_directories(temp_dir);
            std::error_code ec;
            fs::permissions(temp_dir, fs::perms::all, ec);
        }
    }
    void init() {
        builtin_repo.init(*this);
        //repo.init(*this);
    }
    int run_command_line(command_line_parser &cl) {
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

        visit_any(cl.c, [&](auto &b) {
            run_command(cl, b);
        });
        return 0;
    }
    int run_command(command_line_parser &cl, command_line_parser::setup &b) {
        // name sw interpreter as 'swi'?
        if (fs::exists("/bin/sh")) {
            auto fn = "/bin/sw";
            /*write_file_if_different(fn, format(R"(#!/bin/sh
exec {} exec -remove-shebang "$@"
)",
                                               argv[0]));*/ // write argv[0] instead of sw?
            fs::permissions(fn, (fs::perms)0755);
        }
        return 0;
    }
    auto make_solution() {
        solution s{sys, sys.binary_dir, default_host_settings()};
        return s;
    }
    auto make_inputs(auto &&b) {
        if (!b.inputs) {
            b.inputs.value = std::vector<string>{"."};
        }
        inputs_type inputs;
        for (auto &&bi : *b.inputs.value) {
            path p{bi};
            if (!fs::exists(p)) {
                //inputs
                throw std::runtime_error{"does not exist: "s + p.string()};
            }
            if (fs::is_regular_file(p)) {
                SW_UNIMPLEMENTED;
                //direct_build.fns.insert(p);
            } else if (fs::is_directory(p)) {
                if (false) {
                } else if (fs::exists(p / "sw.h")) {
                    input_with_settings is;
                    is.i = specification_file_input{p / "sw.h"};
                    inputs.push_back(is);
                } else if (fs::exists(p / "sw.cpp")) {
                    input_with_settings is;
                    is.i = specification_file_input{p / "sw.cpp"};
                    inputs.push_back(is);
                } else {
                    SW_UNIMPLEMENTED;
                }
            } else {
                throw std::runtime_error{"unknown fs object: "s + p.string()};
            }
        }
        return inputs;
    }
    auto make_build() {
    }
    int run_command(command_line_parser &cl, command_line_parser::build &b) {
        auto cfgdir = storage_dir / "tmp" / "configs";

        //
        config_emitter cfg_host;
        cfg_host.write(cfgdir);

        //
        config_emitter cfg_target;
        cfg_target.write(cfgdir);

        auto gpp = resolve_executable("g++");
        if (gpp) {
            gcc_command g;
            g += *gpp, "--version";
            auto ret = g() && g();
            ret = g() && g() && g();
            std::cerr << g.out.get<string>() << "\n";

            gcc_command g2;
            g2 += *gpp, "asdasd", "-otest";
            ret = g() && g2();
            ret = g2() && g();
            ret = ret || g();
            ret = g2() || g();
            std::cerr << g2.out.get<string>() << "\n";
            std::cerr << g2.err.get<string>() << "\n";

            gcc_command g3;
            g3 += *gpp, "-xc++", "-", "-std=c++26", "-otest", "-lstdc++exp", "-static-libstdc++", "-static-libgcc", "-static", "-lpthread";
            g3.in = string{"#include <print>\nint main(){std::println(\"hello world\");}"};
            g3();
            std::cerr << g3.out.get<string>() << "\n";
            std::cerr << g3.err.get<string>() << "\n";
            g2 |= g3;
            executor ex;
            g2.run(ex);
            g3.run(ex);
            ex.run();
            //std::cerr << g2.out.get<string>() << "\n"; g2.out is redirected
            std::cerr << g2.err.get<string>() << "\n";
            std::cerr << g3.out.get<string>() << "\n";
            std::cerr << g3.err.get<string>() << "\n";
            g2 | g3;
            //std::cerr << g2.out.get<string>() << "\n";
            std::cerr << g2.err.get<string>() << "\n";
            std::cerr << g3.out.get<string>() << "\n";
            std::cerr << g3.err.get<string>() << "\n";
            g2();
            g3();
            //std::cerr << g2.out.get<string>() << "\n";
            std::cerr << g2.err.get<string>() << "\n";
            std::cerr << g3.out.get<string>() << "\n";
            std::cerr << g3.err.get<string>() << "\n";
            g2 | g3 | g;
            std::cerr << g2.err.get<string>() << "\n";
            std::cerr << g3.err.get<string>() << "\n";
            std::cerr << g.out.get<string>() << "\n";
            std::cerr << g.err.get<string>() << "\n";

            // cls && g++ src/client.cpp -Isrc -std=c++26 -lole32 -lOleAut32 -g -O0 -static-libstdc++ -static-libgcc -static -lpthread
            // g++ src/client.cpp -Isrc -std=c++26 -lole32 -lOleAut32 -g -O0 -static-libstdc++ -static-libgcc -static -lpthread
            // mingw command
            gcc_command build_sw;
            build_sw += *gpp, "src/client.cpp", "-Isrc", "-std=c++26",
                // win
                "-lole32", "-lOleAut32",
                // dbg
                //"-g", "-O0",
                "-static-libstdc++", "-static-libgcc", "-static", "-lpthread"
            ;
        }

        build bb;
        bb.inputs = make_inputs(b);
        bb.run();

        return 0;
    }
    int x(command_line_parser &cl, command_line_parser::build &b) {
        /*auto s = make_solution();
        s.binary_dir = temp_sw_directory_path();

        direct_build_input direct_build;
        if (b.inputs) {
            for (auto &&bi : *b.inputs.value) {
                path p{bi};
                if (fs::exists(p) && fs::is_regular_file(p)) {
                    direct_build.fns.insert(p);
                } else {
                    direct_build.fns.clear();
                    break;
                }
            }
        }
        if (!direct_build.fns.empty()) {
            using ttype = executable;
            auto tname = direct_build.fns.begin()->stem().string();
            auto ep = [&](solution &s) {
                auto &t = s.add<ttype>(tname);
                for (auto &&f : direct_build.fns) {
                    t += f;
                }
            };
            input_with_settings is{entry_point{ep, fs::current_path()}};
            auto settings = make_settings(b);
            is.settings.insert(settings.begin(), settings.end());
            s.add_input(is);
            s.build(cl);
            return 0;
        }
        auto cfg_dir = s.binary_dir / "cfg";
        s.binary_dir = cfg_dir;
        auto fn = cfg_dir / "src" / "main.cpp";
        fs::create_directories(fn.parent_path());
        entry_point pch_ep;
        {
            cpp_emitter e;
            e += "#pragma once";
            e += "#include <vector>";
            e += "namespace sw { struct entry_point; }";
            e += "#define SW1_BUILD";
            e += "std::vector<sw::entry_point> sw1_load_inputs();";
            e.include(get_this_file_dir() / "main.cpp");
            //
            auto pch_tmp = temp_sw_directory_path() / "pch";
            auto pch = pch_tmp / "sw.h";
            write_file_if_different(pch, e.s);
            pch_ep.source_dir = pch_tmp;
            pch_ep.binary_dir = pch_tmp;
            pch_ep.build = [pch](solution &s) {
                auto &t = s.add<native_target>("sw_pch");
                t += precompiled_header{pch};
#ifdef __APPLE__
                t += "/usr/local/opt/fmt/include"_idir; // github ci
                t += "/opt/homebrew/include"_idir;      // brew
#endif
            };
        }
        cpp_emitter e;
        string load_inputs = "    return {\n";
        std::vector<input> inputs;
        if (!b.inputs) {
            b.inputs.value = std::vector<string>{"."};
        }
        for (auto &&bi : *b.inputs.value) {
            input i;
            auto check_spec = [&](auto &&fn) {
                auto p = path{bi} / fn;
                if (fs::exists(p)) {
                    i = specification_file_input{p};
                    return true;
                }
                return false;
            };
            if (0 || check_spec("sw.h") || check_spec("sw.cpp") // old compat. After rewrite remove sw.h
                //|| check_spec("sw2.cpp")
                //|| (i = directory_input{"."}, true)
            ) {
                inputs.push_back(i);
            } else {
                throw std::runtime_error{"no inputs found/heuristics not implemented"};
            }
        }
        for (auto &&i : inputs) {
            visit_any(i, [&](specification_file_input &i) {
                auto fn = fs::absolute(i.fn);
                auto fns = normalize_path_and_drive(fn);
                auto fnh = std::hash<string>{}(fns);
                auto nsname = "sw_ns_" + std::to_string(fnh);
                auto ns = e.namespace_(nsname);
                e += "namespace this_namespace = ::" + nsname + ";";
                // add inline ns?
                e.include(fn);
                load_inputs +=
                    "        {&" + nsname + "::build, \"" + normalize_path_and_drive(fn.parent_path()) + "\"},\n";
            });
            e += "";
        }
        load_inputs += "    };";
        e += "std::vector<sw::entry_point> sw1_load_inputs() {";
        e += load_inputs;
        e += "}";
        write_file_if_different(fn, e.s);

        input_with_settings is;
        is.ep.build = [](auto &&s) {
            sw_build b;
            b.build(s);
        };
        is.ep.source_dir = cfg_dir;
        auto dbs = default_build_settings();
        dbs.build_type = build_type::debug{};
        is.settings.insert(dbs);
        s.add_input(is);
        // #ifdef _WIN32
        is.ep = pch_ep;
        s.add_input(is);
        // #endif
        s.load_inputs();
        auto &&t = s.targets.find_first<executable>("sw");
        // #ifdef _WIN32
        auto &&pch = s.targets.find_first<native_target>("sw_pch");
        pch.make_pch();
        t.precompiled_header = pch.precompiled_header;
        t.precompiled_header.create = false;
        // #endif
        s.build(cl);

        if (!fs::exists(t.executable)) {
            throw std::runtime_error{std::format("missing sw1 file", t.executable)};
        }
        auto setup_path = [](auto &&in) {
            auto s = normalize_path_and_drive(in);
            if (is_mingw_shell()) {
                // mingw_drive_letter(s);
            }
            return path{s};
        };
        raw_command c;
        //c.working_directory = setup_path(this_path);
        c += setup_path(t.executable), "-sw1";
        //for (int i = 1; i < argc; ++i) {
            //c += (const char *)argv[i];
        //}
        log_debug("sw1");
        return c.run();*/
        return 1;
    }
    int run_command(command_line_parser &cl, auto &) {
        return 1;
    }

    path pkg_root(auto &&name, auto &&version) const {
        return storage_dir / "pkg" / name / (string)version;
    }
    path mirror_fn(auto &&name, auto &&version) const {
        auto ext = ".zip";
        return storage_dir / "mirror" / (name + "_" + (string)version + ext);
    }

    bool local_mode() const {
        // do we use internet or not
        return false;
    }
};

} // namespace sw
