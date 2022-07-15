#include "storage.h"

#include <regex>
#include <set>

void add_file_to_storage(auto &&s, auto &&f) {
}

void add_transform_to_storage(auto &&s, auto &&f) {
    add_file_to_storage(s,f);
}

struct files_target {
    // unordered?
    using files_t = std::set<path>;
    files_t files;

    void op_from_iterator(auto &&iter, auto &&f) {
        for (auto &&e : iter) {
            if (fs::is_regular_file(e)) {
                f(files, e);
            }
        }
    }
    void operator+=(auto &&iter) {
        op_from_iterator(iter, [](auto &&arr, auto &&f){arr.insert(f);});
    }
    void operator-=(auto &&iter) {
        op_from_iterator(iter, [](auto &&arr, auto &&f){arr.erase(f);});
    }
};
auto operator""_rd(const char *dir, size_t) {
    return fs::directory_iterator{dir};
}
auto operator""_rrd(const char *dir, size_t) {
    return fs::recursive_directory_iterator{dir};
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
auto extract_dir_regex(std::string_view fn) {
    path dir;
    std::string regex_string;
    size_t p = 0;
    do
    {
        auto p0 = p;
        p = fn.find_first_of("/*?+[.\\", p);
        if (p == -1 || fn[p] != '/')
        {
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

        if (s.find_first_of("*?+.[](){}") != -1)
        {
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
template <typename Iter>
auto file_regex(std::string_view fn) {
    auto &&[root,regex] = extract_dir_regex(fn);
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
}
auto operator""_r(const char *s, size_t len) {
    return file_regex<fs::directory_iterator>(std::string_view{s,len});
}
auto operator""_rr(const char *s, size_t len) {
    return file_regex<fs::recursive_directory_iterator>(std::string_view{s,len});
}

auto build_some_package() {
    files_target tgt;
    tgt += "d:/dev/cppan2/client4/src/.*\\.cpp"_r;
    tgt += "d:/dev/cppan2/client4/src/.*\\.h"_rr;
    return tgt;
}

int main() {
    auto tgt = build_some_package();
	file_storage<physical_file_storage_single_file<basic_contents_hash>> fst{ {"single_file2.bin"} };
    for (auto &&f : tgt.files) {
		fst.add(f);
    }
	for (auto&& handle : fst.physical_storage()) {
		handle.copy("myfile.txt");
	}
}
