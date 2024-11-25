#pragma once

#include <filesystem>

namespace sw {

namespace fs = std::filesystem;

// win has '\\' separators, so we create our own path?
// fstreams cannot take custom path as argument
// utf8, '/' separated
struct path {
    static inline constexpr auto preferred_separator = '/';

    std::string value;

    path() = default;
    path(const char *str) : value{str} {
        prepare();
    }
    path(const char *str, size_t len) : path{std::string{str,str+len}} {
     prepare();
    }
    template <auto N>
    path(const char (&str)[N]) : value{str} {
        prepare();
    }
    path(const std::string &v) : value{v} {
        prepare();
    }
    path(const fs::path &p) {
        operator=(p);
    }
#ifdef _WIN32
    path(const wchar_t *str) {
        operator=(fs::path(str));
    }
    path(const std::wstring &v) {
        operator=(fs::path(v));
    }
    path &operator=(const fs::path &p) {
        auto s = p.u8string();
        assign(s.data(), s.size());
        prepare();
        return *this;
    }
#else
    path &operator=(const fs::path &p) {
        value = p.string();
        return *this;
    }
#endif
    path &operator=(const std::string &p) {
        value = p;
        prepare();
        return *this;
    }

    void prepare() {
#ifdef _WIN32
        normalize_path(value);
        lower_drive_letter(value);
#endif
    }

    void assign(const auto *data, size_t sz) {value.assign((const char *)data, sz);}
    void assign(const auto *begin, const auto *end) {assign(begin, end-begin);}
    void clear() {value.clear();}

    auto fspath() const {
        return fs::path((const char8_t *)value.data(), (const char8_t *)value.data() + value.size());
    }
    auto stdpath() const {
        return fs::path((const char8_t *)value.data(), (const char8_t *)value.data() + value.size());
    }
    operator fs::path() const {
        return fspath();
    }
    auto &string()   {return value;}
    const auto &string() const {return value;}
    operator const std::string &() const {
        return value;
    }
#ifdef _WIN32
    auto wstring() const {return fspath().wstring();}
#endif
    auto parent_path() const {
        auto p = *this;
        if (p.value.empty() || p.value.size() == 1 && p.value[0] == preferred_separator) {
            return p;
        }
        auto pos = p.value.rfind(preferred_separator);
        p.value.resize(pos == -1 ? 0 : pos);
        return p;
    }

    auto &operator/=(auto &&v) {
        if (!value.ends_with(preferred_separator)) {
            value += preferred_separator;
        }
        if constexpr (requires {v.value();}) {
            value += v.value();
        } else if constexpr (requires {v.value;}) {
            value += v.value;
        } else {
            value += v;
        }
        return *this;
    }
    auto &operator/=(const path &p) {
        return operator/=(p.value);
    }
    auto &operator/=(const fs::path &p) {
        return operator/=(path{p});
    }
    // objects with operator auto() won't convert to path automatically and fail here
    auto operator/(auto &&v) const {
        auto p = *this;
        p /= v;
        return p;
    }
    auto operator/(const path &p) const {
        return operator/(p.value);
    }
    auto operator/(const fs::path &p) const {
        return operator/(path{p});
    }
    /*auto operator/(const path &v) const {
        auto p = *this;
        p /= v;
        return p;
    }*/
    auto operator+=(auto &&v) {
        value += v;
        return *this;
    }
    auto extension() const {
        auto fn = value.rfind(preferred_separator) + 1;
        //SW_UNIMPLEMENTED;
        return *this;
    }
    path filename() const {
        return value.substr(value.rfind(preferred_separator) + 1);
    }
    auto lexically_normal() const {
        auto fn = value.rfind(preferred_separator) + 1;
        //SW_UNIMPLEMENTED;
        return *this;
    }
    path root_path() const {
        auto fn = value.rfind(preferred_separator) + 1;
        //SW_UNIMPLEMENTED;
        return *this;
    }

    auto empty() const { return value.empty(); }
    auto is_absolute() const {
        // on windows check letter or \\?\ ...
        //SW_UNIMPLEMENTED;
        return !empty() && value[0] == preferred_separator;
    }

    bool operator==(const path &rhs) const {return value == rhs.value;}
    auto operator<=>(const path &rhs) const = default;

    auto data() const {return value.data();}
    auto size() const {return value.size();}

    static void normalize_path(std::string &s) {
        std::replace(s.begin(), s.end(), '\\', '/');
    }
    static void lower_drive_letter(std::string &s) {
        if (s.size() > 1 && s[1] == ':') {
            s[0] = tolower(s[0]);
        }
    }
};
//using fs::path; // consider our own path

} // namespace sw
