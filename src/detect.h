// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2022 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include "helpers.h"
#include "vs_instance_helpers.h"
#include "target.h"
#include "os_base.h"
#include "input.h"

namespace sw {

auto get_windows_arch(const build_settings &bs) {
    auto arch = "x64";
    if (bs.is<arch::x64>()) {
        arch = "x64";
    } else if (bs.is<arch::x86>()) {
        arch = "x86";
    } else if (bs.is<arch::arm64>()) {
        arch = "arm64";
    } else if (bs.is<arch::arm>()) {
        arch = "arm";
    } else {
        throw std::runtime_error{"unknown arch"};
    }
    return arch;
}
auto get_windows_arch(build_settings &bs) {
    auto arch = "x64";
    if (bs.is<arch::x64>()) {
        arch = "x64";
    } else if (bs.is<arch::x86>()) {
        arch = "x86";
    } else if (bs.is<arch::arm64>()) {
        arch = "arm64";
    } else if (bs.is<arch::arm>()) {
        arch = "arm";
    } else {
        throw std::runtime_error{"unknown arch"};
    }
    return arch;
}
auto get_windows_arch(auto &&t) {
    return get_windows_arch(t.bs);
}

struct msvc_instance {
    path root;
    package_version vs_version;

    auto version() const {
        return package_version{package_version::number_version{root.filename().string(), std::get<package_version::number_version>(vs_version.version).extra}};
    }
    auto cl_target(auto &&s) const {
        s.add_entry_point(package_name{"com.Microsoft.VisualStudio.VC.cl"s, version()}, entry_point{[&](decltype(s) &s) {
            auto &t = s.template add<binary_target_msvc>(package_name{"com.Microsoft.VisualStudio.VC.cl"s, version()}, *this);
            t.executable = root / "bin" / ("Host"s + get_windows_arch(s.host_settings())) / get_windows_arch(t) / "cl.exe";
        }});
    }
    auto lib_target(auto &&s) const {
        s.add_entry_point(
            package_name{"com.Microsoft.VisualStudio.VC.lib"s, version()}, entry_point{[&](decltype(s) &s) {
                auto &t = s.template add<binary_target>(package_name{"com.Microsoft.VisualStudio.VC.lib"s, version()});
                t.executable =
                    root / "bin" / ("Host"s + get_windows_arch(s.host_settings())) / get_windows_arch(t) / "lib.exe";
            }});
    }
    auto link_target(auto &&s) const {
        s.add_entry_point(package_name{"com.Microsoft.VisualStudio.VC.link"s, version()}, entry_point{[&](decltype(s) &s) {
            auto &t = s.template add<binary_target>(package_name{"com.Microsoft.VisualStudio.VC.link"s, version()});
            t.executable = root / "bin" / ("Host"s + get_windows_arch(s.host_settings())) / get_windows_arch(t) / "link.exe";
        }});
    }
    auto vcruntime_target(auto &&s) const {
    }
    auto stdlib_target(auto &&s) const {
        s.add_entry_point(
            package_name{"com.Microsoft.VisualStudio.VC.libc"s, version()}, entry_point{[&](decltype(s) &s) {
                // com.Microsoft.VisualStudio.VC.STL?
                auto &t = s.template add<binary_library_target>(package_name{"com.Microsoft.VisualStudio.VC.libc"s, version()});
                t.include_directories.push_back(root / "include");
                auto libdir = root / "lib" / get_windows_arch(t);
                t.link_directories.push_back(libdir);
                auto add_if_exists = [&](auto &&fn) {
                    if (fs::exists(libdir / fn)) {
                        t.link_libraries.push_back(fn);
                    }
                };
                add_if_exists("OLDNAMES.LIB");
                add_if_exists("LEGACY_STDIO_DEFINITIONS.LIB");
                add_if_exists("LEGACY_STDIO_WIDE_SPECIFIERS.LIB");
                if (t.bs.c.runtime.template is<library_type::static_>()) {
                    t.bs.build_type.visit_any(
                        [&](build_type::debug) {
                            t.link_libraries.push_back("LIBCMTD.LIB");
                            t.link_libraries.push_back("LIBVCRUNTIMED.LIB");
                        },
                        [&](build_type::release) {
                            t.link_libraries.push_back("LIBCMT.LIB");
                            t.link_libraries.push_back("LIBVCRUNTIME.LIB");
                        });
                } else {
                    t.bs.build_type.visit_any(
                        [&](build_type::debug) {
                            t.link_libraries.push_back("MSVCRTD.LIB");
                            t.link_libraries.push_back("VCRUNTIMED.LIB");
                        },
                        [&](build_type::release) {
                            t.link_libraries.push_back("MSVCRT.LIB");
                            t.link_libraries.push_back("VCRUNTIME.LIB");
                        });
                }
            }});
        s.add_entry_point(package_name{"com.Microsoft.VisualStudio.VC.libcpp"s, version()}, entry_point{[&](decltype(s) &s) {
            // com.Microsoft.VisualStudio.VC.STL?
            auto &t = s.template add<binary_library_target>(package_name{"com.Microsoft.VisualStudio.VC.libcpp"s, version()});
            t.include_directories.push_back(root / "include");
            auto libdir = root / "lib" / get_windows_arch(t);
            t.link_directories.push_back(libdir);
            auto add_if_exists = [&](auto &&fn) {
                if (fs::exists(libdir / fn)) {
                    t.link_libraries.push_back(fn);
                }
            };
            //add_if_exists("OLDNAMES.LIB");
            //add_if_exists("LEGACY_STDIO_DEFINITIONS.LIB");
            //add_if_exists("LEGACY_STDIO_WIDE_SPECIFIERS.LIB");
            if (t.bs.cpp.runtime.template is<library_type::static_>()) {
                t.bs.build_type.visit_any(
                    [&](build_type::debug) {
                        t.link_libraries.push_back("LIBCONCRTD.LIB");
                        t.link_libraries.push_back("LIBCPMTD.LIB");
                    },
                    [&](build_type::release) {
                        t.link_libraries.push_back("LIBCONCRT.LIB");
                        t.link_libraries.push_back("LIBCPMT.LIB");
                    });
            } else {
                t.bs.build_type.visit_any(
                    [&](build_type::debug) {
                        t.link_libraries.push_back("CONCRTD.LIB");
                        t.link_libraries.push_back("MSVCPRTD.LIB");
                    },
                    [&](build_type::release) {
                        t.link_libraries.push_back("CONCRT.LIB");
                        t.link_libraries.push_back("MSVCPRT.LIB");
                    });
            }
        }});
    }
    //auto root() const { return install_location / "VC" / "Tools" / "MSVC"; }

    void add(auto &&s) {
        cl_target(s);
        lib_target(s);
        link_target(s);
        stdlib_target(s);
    }
};

struct msvc_detector {
    std::vector<msvc_instance> msvc;

    msvc_detector() {
#ifdef _WIN32
        auto instances = enumerate_vs_instances();
        for (auto &&i : instances) {
            path root = i.VSInstallLocation;
            auto preview = i.VSInstallLocation.contains(L"Preview");
            if (preview) {
                //continue;
            }
            auto d = root / "VC" / "Tools" / "MSVC";
            for (auto &&p : fs::directory_iterator{d}) {
                if (!package_version{p.path().filename().string()}.is_branch()) {
                    msvc.emplace_back(d / p.path(), package_version{package_version::number_version{
                                                          path{i.Version}.string(), preview ? "preview"s : ""s}});
                }
            }
        }
#endif
    }
    bool exists() const { return !msvc.empty(); }
    void add(auto &&s) {
        for (auto &&m : msvc) {
            m.add(s);
        }
    }
};
auto &get_msvc_detector() {
    static msvc_detector d;
    return d;
}

// https://en.wikipedia.org/wiki/Microsoft_Windows_SDK
static const char *known_kits[]{"8.1A", "8.1", "8.0", "7.1A", "7.1", "7.0A", "7.0", "6.0A"};
static const auto reg_root = L"SOFTWARE\\Microsoft\\Windows Kits\\Installed Roots";
// list all registry views
static const int reg_access_list[] = {
#ifdef _WIN32
    KEY_READ,
    KEY_READ | KEY_WOW64_32KEY,
    KEY_READ | KEY_WOW64_64KEY,
#endif
};
static const auto win10_kit_name = "10"s;

struct win_kit {
    path kit_root;

    string name;

    std::wstring bdir_subversion;
    std::wstring idir_subversion;
    std::wstring ldir_subversion;

    //string debug_lib;
    //string release_lib;

    std::vector<std::wstring> idirs; // additional idirs
    bool without_ldir = false; // when there's not libs

    void add(auto &&s, const std::wstring &v) {
        auto idir = kit_root / "Include" / idir_subversion;
        if (!fs::exists(idir / name)) {
            //LOG_TRACE(logger, "Include dir " << (idir / name) << " not found for library: " << name);
            return;
        }

        s.add_entry_point(package_name{"com.Microsoft.Windows.SDK."s + name, path{v}.string()},
                          entry_point{[=, *this](decltype(s) &s) {
                              auto &t = s.template add<binary_library_target>(
                                  package_name{"com.Microsoft.Windows.SDK."s + name, path{v}.string()});
                              auto libdir = kit_root / "Lib" / ldir_subversion / name / get_windows_arch(t);
                              if (fs::exists(libdir)) {
                                  t.include_directories.push_back(idir / name);
                                  for (auto &i : idirs) {
                                      t.include_directories.push_back(idir / i);
                                  }
                                  t.link_directories.push_back(libdir);
                                  if (name == "ucrt") {
                                      if (t.bs.c.runtime.template is<library_type::static_>()) {
                                          t.bs.build_type.visit_any(
                                              [&](build_type::debug) {
                                                  t.link_libraries.push_back("LIBUCRTD.LIB");
                                              },
                                              [&](build_type::release) {
                                                  t.link_libraries.push_back("LIBUCRT.LIB");
                                              });
                                      } else {
                                          t.bs.build_type.visit_any(
                                              [&](build_type::debug) {
                                                  t.link_libraries.push_back("UCRTD.LIB");
                                              },
                                              [&](build_type::release) {
                                                  t.link_libraries.push_back("UCRT.LIB");
                                              });
                                      }
                                  } else if (name == "um") {
                                      t.link_libraries.push_back("KERNEL32.LIB");
                                  }
                              } else if (without_ldir) {
                                  t.include_directories.push_back(idir / name);
                                  for (auto &i : idirs) {
                                      t.include_directories.push_back(idir / i);
                                  }
                              } else {
                                  throw std::runtime_error{"libdir not found"};
                                  // LOG_TRACE(logger, "Libdir " << libdir << " not found for library: " << name);
                              }
                          }});
    }

    /*std::vector<sw::PredefinedTarget *> addOld(DETECT_ARGS, sw::OS settings, const sw::Version &v) {
        auto idir = kit_root / "Include";
        if (!fs::exists(idir)) {
            LOG_TRACE(logger, "Include dir " << idir << " not found for kit: " << kit_root);
            return {};
        }

        std::vector<sw::PredefinedTarget *> targets;
        // only two archs
        // (but we have IA64 also)
        for (auto target_arch : {sw::ArchType::x86_64, sw::ArchType::x86}) {
            settings.Arch = target_arch;

            auto ts1 = toTargetSettings(settings);
            sw::TargetSettings ts;
            ts["os"]["kernel"] = ts1["os"]["kernel"];
            ts["os"]["arch"] = ts1["os"]["arch"];

            auto libdir = kit_root / "Lib";
            if (target_arch == sw::ArchType::x86_64)
                libdir /= toStringWindows(target_arch);
            if (fs::exists(libdir)) {
                auto &t = sw::addTarget<sw::PredefinedTarget>(
                    DETECT_ARGS_PASS,
                    sw::LocalPackage(s.getLocalStorage(), sw::PackageId("com.Microsoft.Windows.SDK." + name, v)), ts);
                // t.ts["os"]["version"] = v.toString();

                t.public_ts["properties"]["6"]["system_include_directories"].push_back(idir);
                t.public_ts["properties"]["6"]["system_link_directories"].push_back(libdir);
                targets.push_back(&t);
            } else
                LOG_TRACE(logger, "Libdir " << libdir << " not found for library: " << name);
        }
        return targets;
    }*/

    /*void addTools(DETECT_ARGS) {
        // .rc
        {
            auto p = std::make_shared<sw::SimpleProgram>();
            p->file = kit_root / "bin" / bdir_subversion / toStringWindows(s.getHostOs().Arch) / "rc.exe";
            if (fs::exists(p->file)) {
                auto v = getVersion(s, p->file, "/?");
                sw::TargetSettings ts2;
                auto ts1 = toTargetSettings(s.getHostOs());
                ts2["os"]["kernel"] = ts1["os"]["kernel"];
                auto &rc = addProgram(DETECT_ARGS_PASS, sw::PackageId("com.Microsoft.Windows.rc", v), ts2, p);
            }
            // these are passed from compiler during merge?
            // for (auto &idir : COpts.System.IncludeDirectories)
            // C->system_idirs.push_back(idir);
        }

        // .mc
        {
            auto p = std::make_shared<sw::SimpleProgram>();
            p->file = kit_root / "bin" / bdir_subversion / toStringWindows(s.getHostOs().Arch) / "mc.exe";
            if (fs::exists(p->file)) {
                auto v = getVersion(s, p->file, "/?");
                auto ts1 = toTargetSettings(s.getHostOs());
                sw::TargetSettings ts2;
                ts2["os"]["kernel"] = ts1["os"]["kernel"];
                auto &rc = addProgram(DETECT_ARGS_PASS, sw::PackageId("com.Microsoft.Windows.mc", v), ts2, p);
            }
            // these are passed from compiler during merge?
            // for (auto &idir : COpts.System.IncludeDirectories)
            // C->system_idirs.push_back(idir);
        }
    }*/
};

struct win_sdk_info {
#ifdef _WIN32
    struct reg {
        struct iter {
            HKEY k;
            int id{0};
            LSTATUS r{0};
            std::wstring str;

            iter() {
            }
            iter(HKEY k) : k{k} {
                next();
            }

            bool operator==(int) const {
                return r == ERROR_NO_MORE_ITEMS || r;
            }
            auto &operator*() const {
                return str;
            }
            auto &operator++() {
                next();
                return *this;
            }
            void next() {
                str.clear();
                str.resize(256, 0);
                DWORD sz = str.size();
                r = RegEnumKeyExW(k, id++, str.data(), &sz, 0, 0, 0, 0);
                if (!r) {
                    str = str.c_str();
                }
            }
        };

        HKEY k{};

        reg(HKEY tree, auto &&subtree, auto &&access) {
            if (RegOpenKeyExW(tree, subtree, 0, access | KEY_ENUMERATE_SUB_KEYS, &k)) {
                // throw std::runtime_error("123");
            }
        }
        ~reg() {
            RegCloseKey(k);
        }
        explicit operator bool() const { return !!k; }
        auto begin() { return iter{k}; }
        auto end() { return 0; }

        std::optional<std::wstring> string_value(auto &&value) const {
            std::wstring str(1024, 0);
            DWORD sz = str.size();
            auto r = RegGetValueW(k, 0, value.data(), RRF_RT_REG_SZ, 0, str.data(), &sz);
            if (r == ERROR_MORE_DATA) {
                throw std::runtime_error{"string space is not enough"};
            }
            if (r) {
                return {};
            }
            return str.c_str();
        }
    };
#endif

    using files = std::set<path>;

    files default_sdk_roots;
    files win10_sdk_roots;
    files win81_sdk_roots;
    std::set<std::wstring> kits10;

    win_sdk_info() {
        default_sdk_roots = get_default_sdk_roots();
        win10_sdk_roots = get_windows_kit_root_from_reg(L"10");
        win81_sdk_roots = get_windows_kit_root_from_reg(L"81");
        list_windows_kits();
    }
    auto list_windows10_kits_from_reg() {
#ifdef _WIN32
        for (auto access : reg_access_list) {
            reg r{HKEY_LOCAL_MACHINE, reg_root, access};
            for (auto &&s : r) {
                kits10.insert(s);
            }
        }
#endif
    }
    auto list_windows10_kits_from_fs() {
        for (auto &d : default_sdk_roots) {
            auto p = d / win10_kit_name;
            if (fs::exists(p))
                win10_sdk_roots.insert(p);
        }

        // add more win10 kits
        for (auto &kr10 : win10_sdk_roots) {
            if (!fs::exists(kr10 / "Include"))
                continue;
            // also try directly (kit 10.0.10240 does not register in registry)
            for (auto &d : fs::directory_iterator(kr10 / "Include")) {
                auto k = d.path().filename().wstring();
                if (fs::exists(kr10 / "Lib" / k) && isdigit(k[0])) // sw::Version(k).isVersion())
                    kits10.insert(k);
            }
        }
    }
    void list_windows10_kits() {
        list_windows10_kits_from_reg();
        list_windows10_kits_from_fs();
    }
    void list_windows_kits() {
        list_windows10_kits();
    }

    void add_kits(auto &&s) {
        for (auto &kr10 : win10_sdk_roots) {
            for (auto &v : kits10) {
                add10Kit(s, kr10, v);
            }
        }
    }

    //
    // ucrt - universal CRT
    //
    // um - user mode
    // km - kernel mode
    // shared - some of these and some of these
    //

    void add10Kit(auto &&s, const path &kr, const std::wstring &v) {
        //LOG_TRACE(logger, "Found Windows Kit " + v.toString() + " at " + to_string(normalize_path(kr)));

        // ucrt
        {
            win_kit wk;
            wk.name = "ucrt";
            wk.kit_root = kr;
            wk.idir_subversion = v;
            wk.ldir_subversion = v;
            wk.add(s, v);
        }

        // um + shared
        {
            win_kit wk;
            wk.name = "um";
            wk.kit_root = kr;
            wk.idir_subversion = v;
            wk.ldir_subversion = v;
            wk.idirs.push_back(L"shared");
            //wk.debug_lib = "KERNEL32.LIB";
            //wk.release_lib = "KERNEL32.LIB";
            wk.add(s, v);
        }

        // km
        {
            win_kit wk;
            wk.name = "km";
            wk.kit_root = kr;
            wk.idir_subversion = v;
            wk.ldir_subversion = v;
            wk.add(s, v);
        }

        // winrt
        {
            win_kit wk;
            wk.name = "winrt";
            wk.kit_root = kr;
            wk.idir_subversion = v;
            wk.without_ldir = true;
            wk.add(s, v);
        }

        // cppwinrt
        {
            win_kit wk;
            wk.name = "cppwinrt";
            wk.kit_root = kr;
            wk.idir_subversion = v;
            wk.without_ldir = true;
            wk.add(s, v);
        }

        // tools
        /*{
            win_kit wk;
            wk.kit_root = kr;
            wk.bdir_subversion = v.toString();
            wk.addTools(DETECT_ARGS_PASS);
        }*/
    }
    void addKit(const path &kr, const string &k) const {
        //LOG_TRACE(logger, "Found Windows Kit " + k + " at " + to_string(normalize_path(kr)));

        // tools
        /*{
            win_kit wk;
            wk.kit_root = kr;
            wk.addTools(DETECT_ARGS_PASS);
        }

        auto ver = k;
        if (!k.empty() && k.back() == 'A')
            ver = k.substr(0, k.size() - 1) + ".1";

        // old kits has special handling
        // if (sw::Version(k) < sw::Version(8)) k may have letter 'A', so we can't use such cmp at the moment
        // use simple cmp for now
        // but we pass k as version, so when we need to handle X.XA, we must set a new way
        if (k == "7.1" || k == "7.1A") {
            win_kit wk;
            wk.kit_root = kr;
            wk.name = "um";
            wk.addOld(DETECT_ARGS_PASS, settings, ver);
            wk.name = "km"; // ? maybe km files installed separately?
            wk.addOld(DETECT_ARGS_PASS, settings, ver);
            return;
        }

        // um + shared
        {
            win_kit wk;
            wk.name = "um";
            wk.kit_root = kr;
            if (k == "8.1")
                wk.ldir_subversion = "winv6.3";
            else if (k == "8.0")
                wk.ldir_subversion = "Win8";
            else {
                //LOG_DEBUG(logger, "TODO: Windows Kit " + k + " is not implemented yet. Report this issue.");
            }
            wk.idirs.push_back("shared");
            wk.add(DETECT_ARGS_PASS, settings, ver);
        }

        // km
        {
            win_kit wk;
            wk.name = "km";
            wk.kit_root = kr;
            if (k == "8.1")
                wk.ldir_subversion = "winv6.3";
            else if (k == "8.0")
                wk.ldir_subversion = "Win8";
            else {
                //LOG_DEBUG(logger, "TODO: Windows Kit " + k + " is not implemented yet. Report this issue.");
            }
            wk.add(DETECT_ARGS_PASS, settings, ver);
        }*/
    }

    static files get_program_files_dirs() {
        auto get_env = [](auto p) -> path {
            if (auto e = getenv(p))
                return e;
            return {};
        };
        files dirs;
        for (auto p : {"ProgramFiles(x86)", "ProgramFiles", "ProgramW6432"}) {
            if (auto e = get_env(p); !e.empty())
                dirs.insert(e);
        }
        if (dirs.empty())
            throw std::runtime_error("Cannot get 'ProgramFiles/ProgramFiles(x86)/ProgramW6432' env. vars.");
        return dirs;
    }
    static files get_default_sdk_roots() {
        files dirs;
        for (auto &d : get_program_files_dirs()) {
            auto p = d / "Windows Kits";
            if (fs::exists(p))
                dirs.insert(p);
            // old sdks
            p = d / "Microsoft SDKs";
            if (fs::exists(p))
                dirs.insert(p);
            p = d / "Microsoft SDKs" / "Windows"; // this is more correct probably
            if (fs::exists(p))
                dirs.insert(p);
        }
        return dirs;
    }
    static files get_windows_kit_root_from_reg(const std::wstring &key) {
        files dirs;
#ifdef _WIN32
        for (auto access : reg_access_list) {
            if (reg r{HKEY_LOCAL_MACHINE, reg_root, access}; r) {
                if (auto v = r.string_value(L"KitsRoot" + key); v) {
                    path p = *v;
                    // in registry path are written like 'C:\\Program Files (x86)\\Windows Kits\\10\\'
                    if (p.filename().empty())
                        p = p.parent_path();
                    dirs.insert(p);
                }
            }
        }
#endif
        return dirs;
    }
};

void detect_winsdk(auto &&s) {
    static win_sdk_info sdk;
    sdk.add_kits(s);
}

} // namespace sw
