// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2022 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include "helpers.h"
#include "target.h"

namespace sw {

#ifdef _WIN32

struct msvc_instance {
    path root;
    package_version version;

    auto cl_target() const {
        cl_binary_target t;
        t.package = package_id{"com.Microsoft.VisualStudio.VC.cl"s, root.filename().string()};
        t.exe = root / "bin" / "Hostx64" / "x64" / "cl.exe";
        return t;
    }
    auto link_target() const {
        cl_binary_target t;
        t.package = package_id{"com.Microsoft.VisualStudio.VC.cl"s, root.filename().string()};
        t.exe = root / "bin" / "Hostx64" / "x64" / "link.exe";
        return t;
    }
    auto vcruntime_target() const {
    }
    auto stdlib_target() const {
        binary_library_target t;
        t.include_directories.push_back(root / "include");
        t.link_directories.push_back(root / "lib" / "x64");
        return t;
    }
    //auto root() const { return install_location / "VC" / "Tools" / "MSVC"; }
};

auto detect_msvc1() {
    static auto instances = enumerate_vs_instances();
    std::vector<msvc_instance> cls;
    for (auto &&i : instances) {
        path root = i.VSInstallLocation;
        auto preview = i.VSInstallLocation.contains(L"Preview");
        auto msvc = root / "VC" / "Tools" / "MSVC";
        for (auto &&p : fs::directory_iterator{msvc}) {
            cls.emplace_back(msvc / p.path(),
                package_version{package_version::number_version{path{i.Version}.string(), "preview"s}});
        }
    }
    if (cls.empty()) {
        throw std::runtime_error("empty compilers");
    }
    return cls;
}
const auto &detect_msvc() {
    static auto msvc = detect_msvc1();
    return msvc;
}

// https://en.wikipedia.org/wiki/Microsoft_Windows_SDK
static const char *known_kits[]{"8.1A", "8.1", "8.0", "7.1A", "7.1", "7.0A", "7.0", "6.0A"};
static const auto reg_root = L"SOFTWARE\\Microsoft\\Windows Kits\\Installed Roots";
// list all registry views
static const int reg_access_list[] = {
    KEY_READ,
    KEY_READ | KEY_WOW64_32KEY,
    KEY_READ | KEY_WOW64_64KEY
};
static const auto win10_kit_name = "10"s;

struct win_kit {
    path kit_root;

    string name;

    std::wstring bdir_subversion;
    std::wstring idir_subversion;
    std::wstring ldir_subversion;

    std::vector<std::wstring> idirs; // additional idirs
    bool without_ldir = false; // when there's not libs

    std::optional<binary_library_target> add(const std::wstring &v) {
        auto idir = kit_root / "Include" / idir_subversion;
        if (!fs::exists(idir / name)) {
            //LOG_TRACE(logger, "Include dir " << (idir / name) << " not found for library: " << name);
            return {};
        }

        std::vector<binary_library_target> targets;
        //for (auto target_arch : {sw::ArchType::x86_64, sw::ArchType::x86, sw::ArchType::arm, sw::ArchType::aarch64}) {
        for (auto target_arch : {"x64"}) {
            auto libdir = kit_root / "Lib" / ldir_subversion / name / target_arch;
            if (fs::exists(libdir)) {
                binary_library_target t;
                t.include_directories.push_back(idir / name);
                for (auto &i : idirs)
                    t.include_directories.push_back(idir / i);
                t.link_directories.push_back(libdir);
                //t.public_ts["properties"]["6"]["system_link_directories"].push_back(libdir);
                return t;
                targets.push_back(t);
            /*} else if (without_ldir) {
                auto &t = sw::addTarget<sw::PredefinedTarget>(
                    DETECT_ARGS_PASS,
                    sw::LocalPackage(s.getLocalStorage(), sw::PackageId("com.Microsoft.Windows.SDK." + name, v)), ts);
                // t.ts["os"]["version"] = v.toString();

                t.public_ts["properties"]["6"]["system_include_directories"].push_back(idir / name);
                for (auto &i : idirs)
                    t.public_ts["properties"]["6"]["system_include_directories"].push_back(idir / i);
                targets.push_back(&t);*/
            } else {
                //LOG_TRACE(logger, "Libdir " << libdir << " not found for library: " << name);
            }
        }
        return {};
        return targets[0];
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

    using files = std::set<path>;

    files default_sdk_roots;
    files win10_sdk_roots;
    files win81_sdk_roots;

    binary_library_target ucrt;
    binary_library_target um;

    win_sdk_info() {
        default_sdk_roots = get_default_sdk_roots();
        win10_sdk_roots = get_windows_kit_root_from_reg(L"10");
        win81_sdk_roots = get_windows_kit_root_from_reg(L"81");
    }
    auto list_windows10_kits_from_reg() {
        std::set<std::wstring> kits;
        for (auto access : reg_access_list) {
            reg r{HKEY_LOCAL_MACHINE, reg_root, access};
            for (auto &&s : r) {
                kits.insert(s);
            }
        }
        return kits;
    }
    void list_windows10_kits() {
        auto kits = list_windows10_kits_from_reg();

        auto win10_roots = win10_sdk_roots;
        for (auto &d : default_sdk_roots) {
            auto p = d / win10_kit_name;
            if (fs::exists(p))
                win10_roots.insert(p);
        }

        // add more win10 kits
        for (auto &kr10 : win10_roots) {
            if (!fs::exists(kr10 / "Include"))
                continue;
            // also try directly (kit 10.0.10240 does not register in registry)
            for (auto &d : fs::directory_iterator(kr10 / "Include")) {
                auto k = d.path().filename().wstring();
                if (fs::exists(kr10 / "Lib" / k) && isdigit(k[0]))//sw::Version(k).isVersion())
                    kits.insert(k);
            }
        }

        // now add all kits
        for (auto &kr10 : win10_roots) {
            for (auto &v : kits) {
                add10Kit(kr10, v);
            }
        }
    }
    void list_windows_kits() {
        list_windows10_kits();
    }

    //
    // ucrt - universal CRT
    //
    // um - user mode
    // km - kernel mode
    // shared - some of these and some of these
    //

    void add10Kit(const path &kr, const std::wstring &v) {
        //LOG_TRACE(logger, "Found Windows Kit " + v.toString() + " at " + to_string(normalize_path(kr)));

        // ucrt
        {
            win_kit wk;
            wk.name = "ucrt";
            wk.kit_root = kr;
            wk.idir_subversion = v;
            wk.ldir_subversion = v;
            if (auto o = wk.add(v); o) {
                ucrt = *o;
            }
        }

        // um + shared
        {
            win_kit wk;
            wk.name = "um";
            wk.kit_root = kr;
            wk.idir_subversion = v;
            wk.ldir_subversion = v;
            wk.idirs.push_back(L"shared");
            if (auto o = wk.add(v); o) {
                um = *o;
            }
            //for (auto t : wk.add(v))
                //t->public_ts["properties"]["6"]["system_link_libraries"].push_back("KERNEL32.LIB");
        }

        // km
        {
            win_kit wk;
            wk.name = "km";
            wk.kit_root = kr;
            wk.idir_subversion = v;
            wk.ldir_subversion = v;
            wk.add(v);
        }

        // winrt
        {
            win_kit wk;
            wk.name = "winrt";
            wk.kit_root = kr;
            wk.idir_subversion = v;
            wk.without_ldir = true;
            wk.add(v);
        }

        // cppwinrt
        {
            win_kit wk;
            wk.name = "cppwinrt";
            wk.kit_root = kr;
            wk.idir_subversion = v;
            wk.without_ldir = true;
            wk.add(v);
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
        return dirs;
    }
};

auto detect_winsdk1() {
    win_sdk_info sdk;
    sdk.list_windows_kits();
    return sdk;
}
const auto &detect_winsdk() {
    static auto sdk = detect_winsdk1();
    return sdk;
}

#endif

}
