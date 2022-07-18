#include "storage.h"

#include <regex>
#include <set>

void add_file_to_storage(auto &&s, auto &&f) {
}

void add_transform_to_storage(auto &&s, auto &&f) {
    add_file_to_storage(s,f);
}

[[nodiscard]]
std::string replace(const std::string &str, const std::string &oldstr, const std::string &newstr, int count = -1) {
    int sofar = 0;
    int cursor = 0;
    std::string s(str);
    std::string::size_type oldlen = oldstr.size(), newlen = newstr.size();
    cursor = s.find(oldstr, cursor);
    while (cursor != -1 && cursor <= (int)s.size()) {
        if (count > -1 && sofar >= count) {
            break;
        }
        s.replace(cursor, oldlen, newstr);
        cursor += (int)newlen;
        if (oldlen != 0) {
            cursor = s.find(oldstr, cursor);
        } else {
            ++cursor;
        }
        ++sofar;
    }
    return s;
}

struct file_regex {
    std::string_view str;
    bool recursive;

    void op(auto &&rootdir, auto &&f) {
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

template <typename F>
struct appender {
    F f;
    auto operator,(auto &&v) {
        f(v);
        return std::move(*this);
    }
};

struct files_target {
    // unordered?
    using files_t = std::set<path>;
    path source_dir;
    files_t files;

    void op(file_regex &r, auto &&f) {
        r.op(source_dir, [&](auto &&iter) {
            for (auto &&e : iter) {
                if (fs::is_regular_file(e)) {
                    f(files, e);
                }
            }
        });
    }
    void op(const path &p, auto &&f) {
        f(files, p);
    }
    /*void op(const char *s, auto &&f) {
        op(path{s}, f);
    }*/
    auto operator+=(auto &&iter) {
        op(iter, [](auto &&arr, auto &&f){arr.insert(f);});
        return appender{[&](auto &&v){operator+=(v);}};
    }
    auto operator-=(auto &&iter) {
        op(iter, [](auto &&arr, auto &&f){arr.erase(f);});
        return appender{[&](auto &&v){operator-=(v);}};
    }
};
auto operator""_dir(const char *dir, size_t) {
    return fs::directory_iterator{dir};
}
auto operator""_rdir(const char *dir, size_t) {
    return fs::recursive_directory_iterator{dir};
}
auto operator""_r(const char *s, size_t len) {
    return file_regex(std::string_view{s,len},false);
}
auto operator""_rr(const char *s, size_t len) {
    return file_regex(std::string_view{s,len}, true);
}

struct cl_binary {
    // rule
    // command
};
struct mingw_binary {
};

struct rule_target : files_target {
    // commands?
};

struct solution {
    path root;
    // config
};

auto build_some_package(solution &s) {
    files_target tgt;
    tgt.source_dir = s.root;
    tgt +=
        "src/main.cpp",
        "src/.*\\.cpp"_r,
        "src/.*\\.h"_rr
        ;
    return tgt;
}

auto build_some_package2(solution &s) {
    rule_target tgt;
    /*tgt.root = s.root;
    tgt +=
        "src/.*\\.cpp"_r,
        "src/.*\\.h"_rr
        ;*/
    //tgt += cl_binary{};
    return tgt;
}

int main() {
    solution s{"d:/dev/cppan2/client4"};
    auto tgt = build_some_package(s);
    auto tgt2 = build_some_package2(s);
	file_storage<physical_file_storage_single_file<basic_contents_hash>> fst{ {"single_file2.bin"} };
    for (auto &&f : tgt.files) {
		fst.add(f);
    }
	for (auto&& handle : fst.physical_storage()) {
		handle.copy("myfile.txt");
	}
}
