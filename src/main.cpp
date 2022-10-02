#include "command.h"
#include "storage.h"
#include "vs_instance_helpers.h"
#include "package.h"

#include <map>
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

struct files_target {
    using files_t = std::set<path>; // unordered?
    path source_dir;
    files_t files;

    void op(file_regex &r, auto &&f) {
        r(source_dir, [&](auto &&iter) {
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
    auto operator+=(auto &&iter) {
        op(iter, [&](auto &&arr, auto &&f){arr.insert(source_dir / f);});
        return appender{[&](auto &&v){operator+=(v);}};
    }
    auto operator-=(auto &&iter) {
        op(iter, [&](auto &&arr, auto &&f){arr.erase(source_dir / f);});
        return appender{[&](auto &&v){operator-=(v);}};
    }
    auto range() const { return files | std::views::transform([&](auto &&p){ return source_dir / p; }); }
    auto begin() const { return iter_with_range{range()}; }
    auto end() const { return files.end(); }
};

struct rule_flag {
    std::set<void *> rules;

    template <typename T>
    auto get_rule_tag() {
        static void *p;
        return (void *)&p;
    }
    template <typename T>
    bool contains(T &&) {
        return rules.contains(get_rule_tag<std::remove_cvref_t<std::remove_pointer_t<T>>>());
    }
    template <typename T>
    auto insert(T &&) {
        return rules.insert(get_rule_tag<std::remove_cvref_t<std::remove_pointer_t<T>>>());
    }
};

struct definition {
    string key;
    std::variant<string, bool> value; // value/undef

    operator string() const {
        return visit(value, overload{
            [&](const string &v){ return "-D" + key + "=" + v; },
            [&](bool){ return "-U" + key; }
        });
    }
};
struct compile_options_t {
    std::vector<definition> definitions;
    std::vector<path> include_directories;
};
struct link_options_t {};

// binary_target_package?
struct cl_binary_target : compile_options_t, link_options_t {
    sw::package_id package;
    path exe;
};

auto detect_cl_compilers() {
    static auto instances = enumerate_vs_instances();
    std::vector<cl_binary_target> cls;
    for (auto &&i : instances) {
        path root = i.VSInstallLocation;
        auto msvc = root / "VC" / "Tools" / "MSVC";
        auto i = fs::directory_iterator{msvc};
        if (i == fs::directory_iterator{}) {
            continue;
        }
        auto &t = cls.emplace_back();
        t.package = sw::package_id{"com.Microsoft.VisualStudio.VC.cl"s,i->path().string()};
        t.exe = msvc / i->path() / "bin" / "Hostx64" / "x64" / "cl.exe";
        //i.VSInstallLocation.contains(L"Preview");

        t.include_directories.push_back(msvc / i->path() / "include");
    }
    return cls;
}

struct sources_rule {
    void operator()(auto &tgt) const {
        for (auto &&f : tgt) {
            if (f.extension() == ".cpp") {
                tgt.processed_files[f].insert(this);
            }
        }
    }
};
struct cl_exe : compile_options_t {
    cl_binary_target compiler;

    cl_exe() {
        auto instances = detect_cl_compilers();
        if (instances.empty()) {
            throw std::runtime_error("empty compilers");
        }
        compiler = *instances.begin();
    }
    void init(const path &p) {
    }

    void operator()(auto &tgt) const {
        for (auto &&[f,rules] : tgt.processed_files) {
            if (!rules.contains(this)) {
                auto out = f.filename() += ".obj";
                command2 c;
                c +=
                    compiler.exe,
                    "-nologo",
                    "-c",
                    "-std:c++latest",
                    "-EHsc",
                    f,
                    "-Fo" + out.string()
                    ;
                for (auto &&d : tgt.compile_options.definitions) {
                    c += (string)d;
                }
                for (auto &&i : tgt.compile_options.include_directories) {
                    c += "-I" + i.string();
                }
                for (auto &&d : compiler.definitions) {
                    c += (string)d;
                }
                for (auto &&i : compiler.include_directories) {
                    c += "-I" + i.string();
                }
                c.inputs.insert(f);
                c.outputs.insert(out);
                tgt.commands.push_back(c);
                rules.insert(this);
            }
        }
    }
};
struct link_exe {
    void operator()(auto &tgt) const {
    }
};

using rule_types = types<sources_rule, cl_exe, link_exe>;
using rule = decltype(make_variant(rule_types{}))::type;

struct rule_target : files_target {
    std::vector<rule> rules;
    std::map<path, rule_flag> processed_files;
    std::vector<command2> commands;

    compile_options_t compile_options;
    link_options_t link_options;

    /*void add_rule(auto &&r) {
        r(*this);
    }*/
    void add_rule(const rule &r) {
        std::visit([&](auto &&v){v(*this);}, r);
    }
    using files_target::operator+=;
    template <typename T>
    auto operator+=(T &&r) requires requires {requires rule_types::contains<T>();} {
        add_rule(r);
        return appender{[&](auto &&v){operator+=(v);}};
    }

    void operator()() {
        for (auto &&c : commands) {
            c.run();
        }
    }
};

struct solution {
    path root;
    // config
};

auto build_some_package(solution &s) {
    files_target tgt;
    tgt.source_dir = s.root;
    tgt +=
        "src"_rdir,
        "src/main.cpp",
        "src/.*\\.cpp"_r,
        "src/.*\\.h"_rr
        ;
    return tgt;
}

auto build_some_package2(solution &s) {
    rule_target tgt;
    tgt.source_dir = s.root;
    tgt +=
        "src"_rdir,
        "src/main.cpp",
        "src/.*\\.cpp"_r,
        "src/.*\\.h"_rr
        ;
    tgt += sources_rule{};
    tgt += cl_exe{};
    tgt();
    return tgt;
}

int main1() {
    solution s{"d:/dev/cppan2/client4"};
    auto tgt = build_some_package(s);
    auto tgt2 = build_some_package2(s);
	file_storage<physical_file_storage_single_file<basic_contents_hash>> fst{ {"single_file2.bin"} };
    fst += tgt;
	for (auto &&handle : fst) {
		handle.copy("myfile.txt");
	}
    return 0;
}

int main() {
    try { return main1();
    } catch (std::exception &e) {
        std::cerr << e.what();
    } catch (...) {
        std::cerr << "unknown exception";
    }
}
