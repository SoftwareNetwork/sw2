#pragma once

#include "fs.h"
#include "mmap.h"
#include "../crypto/common.h"

#include <algorithm>
#include <fstream>
#include <ranges>
#include <set>

#ifndef _WIN32
#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace sw {

inline path get_home_directory() {
#ifdef _WIN32
    wchar_t *w;
    size_t len;
    auto err = _wdupenv_s(&w, &len, L"USERPROFILE");
    if (err) {
        throw std::runtime_error{"Cannot get user's home directory (%USERPROFILE%)"};
    }
    path home = w;
    free(w);
#else
    // prefer this way
    auto p = getpwuid(getuid());
    if (p) {
        return p->pw_dir;
    }
    auto home = getenv("HOME");
    if (!home) {
        throw std::runtime_error{"Cannot get user's home directory ($HOME)"};
    }
#endif
    return home;
}

auto read_file(const path &fn) {
    mmap_file<uint8_t> m{fn};
    return std::string(m.p, m.p+m.sz);
}
auto write_file(const path &fn, auto &&v) {
    if (!fs::exists(fn)) {
        fs::create_directories(fn.parent_path());
        std::ofstream{fn.fspath()};
    }
    fs::resize_file(fn, v.size());
    mmap_file<uint8_t> m{fn, mmap_file<uint8_t>::rw{}};
    memcpy(m.p, v.data(), v.size());
}

template <typename T>
auto read_file_or_default(const path &fn, T &&default_) {
    if (!fs::exists(fn)) {
        return default_;
    }
    return read_file(fn);
}
template <typename T>
auto read_file_or_write_default(const path &fn, T &&default_) {
    if (!fs::exists(fn)) {
        write_file(fn, default_);
        return default_;
    }
    return read_file(fn);
}

struct abspath : path {
    using base = path;

    abspath() = default;
    abspath(const base &p) : base{p} {
        init();
    }
    abspath(auto &&p) : base{p} {
        init();
    }
    abspath(const abspath &) = default;

    void init() {
        // we need target os concept and do this when needed
/*#ifdef _WIN32
        std::wstring s = fs::absolute(p);
        std::transform(s.begin(), s.end(), s.begin(), towlower);
        base::operator=(s);
#else*/
        base::operator=(fs::absolute(*this));
        base::operator=(fs::absolute(*this).lexically_normal()); // on linux absolute does not remove last '.'?
//#endif
    }
};

string &&normalize_path(string &&s) {
    std::replace(s.begin(), s.end(), '\\', '/');
    return std::move(s);
}
void prepare_drive_letter(string &s) {
    if (s.size() > 1 && s[1] == ':') {
        s[0] = toupper(s[0]);
    }
}
void mingw_drive_letter(string &s) {
    if (s.size() > 1 && s[1] == ':') {
        s[0] = toupper(s[0]);
        s = "/"s + s[0] + s.substr(2);
    }
}
// also /cygdrive/
auto normalize_path(const path &p) {
    auto fn = p.string();
    std::replace(fn.begin(), fn.end(), '\\', '/');
    return fn;
}
auto normalize_path_and_drive(const path &p) {
    auto fn = normalize_path(p);
    prepare_drive_letter(fn);
    return fn;
}

/*auto read_file(const path &fn) {
    auto sz = fs::file_size(fn);
    string s(sz, 0);
    FILE *f = fopen(fn.string().c_str(), "rb");
    fread(s.data(), s.size(), 1, f);
    fclose(f);
    return s;
}

void write_file(const path &fn, const string &s = {}) {
    fs::create_directories(fn.parent_path());

    FILE *f = fopen(fn.string().c_str(), "wb");
    fwrite(s.data(), s.size(), 1, f);
    fclose(f);
}*/

void write_file_if_different(const path &fn, const string &s) {
    if (fs::exists(fn) && read_file(fn) == s) {
        return;
    }
    write_file(fn, s);
}

void write_file_once(const path &fn, const string &content, const path &lock_dir) {
    auto sha3 = [](auto &&d) {
        return digest<crypto::sha3<256>>(d);
    };

    auto h = sha3(content);

    auto hf = sha3(normalize_path(fn));
    const auto once = lock_dir / (hf + ".once");
    const auto lock = lock_dir / hf;

    if (!fs::exists(once) || h != read_file(once) || !fs::exists(fn)) {
        // ScopedFileLock fl(lock);
        write_file_if_different(fn, content);
        write_file_if_different(once, h);
    }
}

std::optional<path> resolve_executable(auto &&exe) {
#ifdef _WIN32
    auto p = getenv("Path"); // use W version?
    auto delim = ';';
    auto exts = {".exe", ".bat", ".cmd", ".com"}; // use PATHEXT? it has different order
#else
    auto p = getenv("PATH");
    auto delim = ':';
    auto exts = {""};
#endif
    if (!p) {
        return {};
    }
    string p2 = p;
    for (const auto word : std::views::split(p2, delim) | std::views::transform([](auto &&word) {
                               return std::string_view{word.begin(), word.end()};
                           })) {
        auto p = path{word} / exe;
        for (auto &e : exts) {
            if (fs::exists(path{p} += e)) {
                return path{p} += e;
            }
        }
    }
    return {};
}

inline auto is_c_file(const path &fn) {
    static std::set<string> exts{".c", ".m"}; // with obj-c, separate call?
    return exts.contains(fn.extension().string());
}
inline auto is_cpp_file(const path &fn) {
    static std::set<string> exts{".cpp", ".cxx", ".mm"}; // with obj-c++, separate call?
    return exts.contains(fn.extension().string());
}

inline auto temp_sw_directory_path() {
    return fs::temp_directory_path() / "sw";
}

} // namespace sw

template <>
struct std::formatter<sw::path> : formatter<std::string> {
    auto format(const sw::path &p, format_context &ctx) const {
        return std::formatter<std::string>::format(p.string(), ctx);
    }
};

template <>
struct std::hash<::sw::path> {
    size_t operator()(const ::sw::path &p) {
#ifdef _WIN32
        return std::hash<std::filesystem::path>()(p.fspath());
#else
        return std::hash<sw::string>()(p.string());
#endif
    }
};

template <>
struct std::hash<::sw::abspath> {
    size_t operator()(const ::sw::abspath &p) {
        return std::hash<sw::path>()(p);
    }
};
