#pragma once

#include "vs_instance_helpers.h"
//#include "../builtin/detect.h"

struct cl_exe_rule1 {
    path executable;
    package_version vs_version;
    bool clang{};

    void operator()(auto &&tgt)
        requires requires { tgt.compile_options; }
    {
        auto objext = tgt.bs.os.visit([](auto &&v) -> string_view {
            if constexpr (requires { v.object_file_extension; }) {
                return v.object_file_extension;
            } else {
                throw std::runtime_error{"no object extension"};
            }
        });

        auto add_flags = [&](auto &c, auto &&f) {
            c.old_includes = vs_version < package_version{16, 7};
            // https://developercommunity.visualstudio.com/t/Enable-bigobj-by-default/1031214
            c += "-bigobj"; // we use this by default
            //  -Wa,-mbig-obj
            c += "-FS"; // ForceSynchronousPDBWrites
            c += "-Zi"; // DebugInformationFormatType::ProgramDatabase
            tgt.bs.build_type.visit(
                [&](build_type::debug) {
                    c += "-Od";
                },
                [&](auto) {
                    c += "-O2";
                });
            auto mt_md = [&](auto &&obj) {
                if (obj.template is<library_type::static_>()) {
                    tgt.bs.build_type.visit(
                        [&](build_type::debug) {
                            c += "-MTd";
                        },
                        [&](auto) {
                            c += "-MT";
                        });
                } else {
                    tgt.bs.build_type.visit(
                        [&](build_type::debug) {
                            c += "-MDd";
                        },
                        [&](auto) {
                            c += "-MD";
                        });
                }
            };
            if (is_c_file(f)) {
                c += "-TC";
                // mt_md(tgt.bs.c.runtime);
            } else if (is_cpp_file(f)) {
                c += "-TP";
                c += "-EHsc"; // enable for c too?
                c += "-std:c++latest";
                // mt_md(tgt.bs.cpp.runtime);
            }
            add_compile_options(tgt.merge_object(), c);
            for (auto &&i : tgt.merge_object().force_includes) {
                c += "-FI" + i.p.string();
            }
        };

        if constexpr (requires { tgt.precompiled_header; }) {
            if (!tgt.precompiled_header.header.empty()) {
                if (tgt.precompiled_header.create) {
                    auto &r = tgt.processed_files[tgt.precompiled_header.header];
                    if (!r.contains(this)) {
                        cl_exe_command c;
                        c.working_directory = tgt.binary_dir / "obj";
                        c += executable, "-nologo", "-c";
                        c.inputs.insert(executable);
                        auto out = tgt.precompiled_header.obj;
                        c.name_ = format_log_record(tgt, "/[pch]");
                        auto f = tgt.precompiled_header.cpp;
                        c += "-FI" + tgt.precompiled_header.header.string();
                        add_flags(c, f);
                        c += f, "-Fo" + out.string();
                        c += "-Yc" + tgt.precompiled_header.header.string();
                        c += "-Fp" + tgt.precompiled_header.pch.string();
                        c += "-Fd" + tgt.precompiled_header.pdb.string();
                        c.inputs.insert(f);
                        c.outputs.insert(out);
                        c.outputs.insert(tgt.precompiled_header.pch);
                        // c.outputs.insert(tgt.precompiled_header.pdb);
                        tgt.commands.emplace_back(std::move(c));
                        r.insert(this);
                    }
                }
                if (tgt.precompiled_header.use) {
                    tgt.processed_files[tgt.precompiled_header.obj];
                }
            }
        }
        for (auto &&[f, rules] : tgt.processed_files) {
            if (rules.contains(this) || !(is_c_file(f) || is_cpp_file(f))) {
                continue;
            }
            cl_exe_command c;
            c.working_directory = tgt.binary_dir / "obj";
            c += executable, "-nologo", "-c";
            c.inputs.insert(executable);
            auto out = tgt.binary_dir / "obj" / f.filename() += objext;
            c.name_ = format_command_name(tgt, f);
            if constexpr (requires { tgt.precompiled_header; }) {
                if (!tgt.precompiled_header.header.empty() && tgt.precompiled_header.use) {
                    c += "-FI" + tgt.precompiled_header.header.string();
                    c += "-Fp" + tgt.precompiled_header.pch.string();
                    c += "-Yu" + tgt.precompiled_header.header.string();
                    c += "-Fd" + tgt.precompiled_header.pdb.string();
                    c.inputs.insert(tgt.precompiled_header.header);
                    c.inputs.insert(tgt.precompiled_header.pch);
                    // c.inputs.insert(tgt.precompiled_header.pdb);
                }
            }
            add_flags(c, f);
            c += f, "-Fo" + out.string();
            c.inputs.insert(f);
            c.outputs.insert(out);
            tgt.commands.emplace_back(std::move(c));
            rules.insert(this);
        }
    }
};
struct lib_exe_rule1 {
    void operator()(auto &&tgt, auto &&librarian)
        requires requires { tgt.library; }
    {
        io_command c;
        c.err = ""s;
        c.out = ""s;
        c += librarian.executable, "-nologo";
        c.inputs.insert(librarian.executable);
        c.name_ = format_log_record(tgt, tgt.library.extension().string());
        c += "-OUT:" + tgt.library.string();
        c.outputs.insert(tgt.library);
        bool has_files{};
        for (auto &&[f, rules] : tgt.processed_files) {
            if (rules.contains(this)) {
                continue;
            }
            if (f.extension() == ".obj") {
                has_files = true;
                c += f;
                c.inputs.insert(f);
                rules.insert(this);
            }
        }
        if (!has_files) {
            return;
        }
        tgt.commands.emplace_back(std::move(c));
    }
};
struct link_exe_rule1 {
    void operator()(auto &&tgt, auto &&linker)
        requires requires { tgt.link_libraries; }
    {
        auto objext = tgt.bs.os.visit([](auto &&v) -> string_view {
            if constexpr (requires { v.object_file_extension; }) {
                return v.object_file_extension;
            } else {
                throw std::runtime_error{"no object extension"};
            }
        });

        io_command c;
        c.err = ""s;
        c.out = ""s;
        c += linker.executable, "-nologo";
        c.inputs.insert(linker.executable);
        if constexpr (requires { tgt.executable; }) {
            c.name_ = format_log_record(tgt, tgt.executable.extension().string());
            c += "-OUT:" + tgt.executable.string();
            c.outputs.insert(tgt.executable);
        } else if constexpr (requires { tgt.library; }) {
            c.name_ = format_log_record(tgt, tgt.library.extension().string());
            if (!tgt.implib.empty()) {
                c += "-DLL";
                c += "-IMPLIB:" + tgt.implib.string();
                c.outputs.insert(tgt.implib);
            }
            c += "-OUT:" + tgt.library.string();
            c.outputs.insert(tgt.library);
        } else {
        }
        bool has_files{};
        for (auto &&[f, rules] : tgt.processed_files) {
            if (rules.contains(this)) {
                continue;
            }
            if (f.extension() == objext) {
                has_files = true;
                c += f;
                c.inputs.insert(f);
                rules.insert(this);
            }
        }
        if (!has_files) {
            return;
        }
        c += "-NODEFAULTLIB";
        tgt.bs.build_type.visit_any(
            [&](build_type::debug) {
                c += "-DEBUG:FULL";
            },
            [&](build_type::release) {
                c += "-DEBUG:NONE";
                // c += "-PDB:" + tgt.out.string(); // only for !debug?
            });
        auto add = [&](auto &&v) {
            for (auto &&i : v.link_directories) {
                c += "-LIBPATH:" + i.string();
            }
            for (auto &&d : v.link_libraries) {
                c += d;
            }
            for (auto &&d : v.system_link_libraries) {
                c += d;
            }
            for (auto &&d : v.link_options) {
                c += d;
            }
        };
        add(tgt.merge_object());
        tgt.commands.emplace_back(std::move(c));
    }
};

struct msvc_instance1 {
    path root;
    package_version vs_version;

    auto version() const {
        return package_version{package_version::number_version{
            root.filename().string(), std::get<package_version::number_version>(vs_version.version).extra}};
    }
    auto get_program_path(auto &&host, auto &&target) const {
        return root / "bin" / ("Host"s + host) / target;
    }
    auto prog(auto &&h, auto &&t, auto &&p) const {
        return get_program_path(h, t) / p;
    }
    auto cl_exe(auto &&h, auto &&t) const {
        return prog(h,t, "cl.exe");
    }
    bool has_cl_exe(auto &&h, auto &&t) const {
        return fs::exists(cl_exe(h,t));
    }

    bool init(auto &&swctx) {
        // check only this arch
        auto n = get_windows_arch_name(swctx.sys.arch);
        auto cl = cl_exe(n,n);
        if (!fs::exists(cl)) {
            return false;
        }
        settings s;
        s["arch"] = arch_type{swctx.sys.arch};
        swctx.sys.packages[{"com.Microsoft.VisualStudio.VC.cl",vs_version}].emplace(std::move(s), cl_exe_rule1{cl,vs_version});
        //swctx.sys.packages[{"cl",vs_version}].emplace(std::move(s), cl_exe_rule1{cl,vs_version});
        //swctx.sys.packages[{"com.Microsoft.VisualStudio.VC.lib",vs_version}].emplace(std::move(s), cl_exe_rule1{prog("lib.exe"),vs_version});
        //swctx.sys.packages[{"com.Microsoft.VisualStudio.VC.link",vs_version}].emplace(std::move(s), cl_exe_rule1{prog("link.exe"),vs_version});
        return true;
    }
};

struct msvc {
    std::vector<msvc_instance1> msvc;

    void detect(auto &&swctx) {
        auto instances = enumerate_vs_instances();
        for (auto &&i : instances) {
            path root = i.VSInstallLocation;
            auto preview = i.VSInstallLocation.contains(L"Preview");
            if (preview) {
                // continue;
            }
            auto d = root / "VC" / "Tools" / "MSVC";
            for (auto &&p : fs::directory_iterator{d}) {
                if (!package_version{p.path().filename().string()}.is_branch()) {
                    msvc.emplace_back(d / p.path(), package_version{package_version::number_version{
                                                        path{i.Version}.string(), preview ? "preview"s : ""s}});
                    msvc.back().init(swctx);
                }
            }
        }
    }
};

