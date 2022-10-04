#pragma once

#include "helpers.h"

namespace sw {

struct file_regex {
    std::string str;
    bool recursive;

    void operator()(auto &&rootdir, auto &&f) {
        auto &&[root,regex] = extract_dir_regex(str);
        if (root.is_absolute()) {
            throw;
        }
        root = rootdir / root;
        auto range = [&]<typename Iter>() {
            return
                // add caching? yes
                // add win fast path? yes
                std::ranges::owning_view{Iter{root}}
                | std::views::filter([dir = root.string(), r = std::regex{regex}](auto &&el){
                    auto s = el.path().string();
                    if (s.starts_with(dir)) {
                        // eat one more slash
                        s = s.substr(dir.size() + 1);
                    } else {
                        throw;
                    }
                    return std::regex_match(s, r);
                })
                ;
        };
        if (recursive) {
            f(range.operator()<fs::recursive_directory_iterator>());
        } else {
            f(range.operator()<fs::directory_iterator>());
        }
    }
    static auto extract_dir_regex(std::string_view fn) {
        path dir;
        std::string regex_string;
        size_t p = 0;
        do
        {
            auto p0 = p;
            p = fn.find_first_of("/*?+[.\\", p);
            if (p == -1 || fn[p] != '/') {
                regex_string = fn.substr(p0);
                return std::tuple{dir, regex_string};
            }

            // scan first part for '\.' pattern that is an exact match for point
            // other patterns? \[ \( \{

            auto sv = fn.substr(p0, p++ - p0);
            string s{sv.begin(), sv.end()};
            // protection of valid path symbols into regex
            s = replace(s, "\\.", ".");
            s = replace(s, "\\[", "[");
            s = replace(s, "\\]", "]");
            s = replace(s, "\\(", "(");
            s = replace(s, "\\)", ")");
            s = replace(s, "\\{", "{");
            s = replace(s, "\\}", "}");

            if (s.find_first_of("*?+.[](){}") != -1) {
                regex_string = fn.substr(p0);
                return std::tuple{dir, regex_string};
            }

            // windows fix
            if (!s.empty() && s.back() == ':') {
                s += '/';
            }

            dir /= s;
        } while (1);
    }
};

auto operator""_dir(const char *s, size_t len) {
    return file_regex(std::string{s,len} + "/.*",false);
}
auto operator""_rdir(const char *s, size_t len) {
    return file_regex(std::string{s,len} + "/.*", true);
}
auto operator""_r(const char *s, size_t len) {
    return file_regex(std::string{s,len},false);
}
auto operator""_rr(const char *s, size_t len) {
    return file_regex(std::string{s,len}, true);
}

/*auto operator""_slib(const char *s, size_t len) {
    return file_regex(std::string{s, len}, true);
}*/

}
