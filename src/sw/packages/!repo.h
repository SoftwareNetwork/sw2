struct git {
    std::string dl_url;

    git(auto &&url, auto &&version_tag) {
        dl_url = std::format("{}/archive/refs/tags/{}", url, version_tag);
    }
};
struct github : git {
    github(auto &&user, auto &&repo, auto &&version_tag)
        : git{"https://github.com/"s + user + "/" + repo,version_tag} {
    }
    github(auto &&user_repo, auto &&version_tag)
        : git{"https://github.com/"s + user_repo, version_tag} {
    }
};

namespace sw::self_build {
#include "../sw.h"
}
#include "zlib.h"
#include "png_1.0.5.h"
#include "png.h"

std::string replace_all(std::string s, const std::string &pattern, const std::string &repl) {
    size_t pos;
    while ((pos = s.find(pattern)) != -1) {
        s = s.substr(0, pos) + repl + s.substr(pos + pattern.size());
    }
    return s;
}

struct file_regex_op {
    enum {
        add,
        remove,
    };
    file_regex r;
    int operation{add};
};
using file_regex_ops = std::vector<file_regex_op>;

void unpack(const path &arch, const path &dir) {
    fs::create_directories(dir);

    raw_command c;
    c.working_directory = dir;
    c += resolve_executable("tar"), "-xvf", arch;
    log_info("extracting {}", arch);
    c.run();
}

template <typename Source, typename Version>
struct package {
    std::string name;
    Source source;
    Version version;

    //
    path source_dir;
    std::set<path> files;
    path arch;

    ~package() {
        //std::error_code ec;
        fs::remove(arch);
    }
    void download(auto &&wdir) {
        auto ext = ".zip";
        arch = wdir / (name + ext);
        fs::create_directories(wdir);
        {
            raw_command c;
            // c += resolve_executable("git"), "clone", url, "1";
            auto url = source.dl_url + ext;
            url = replace_all(url, "{v}", version);
            url = replace_all(url, "{M}", std::to_string(version[0]));
            url = replace_all(url, "{m}", std::to_string(version[1]));
            url = replace_all(url, "{p}", std::to_string(version[2]));
            if (version[2]) {
                url = replace_all(url, "{po}", "."s + std::to_string(version[2]));
            } else {
                url = replace_all(url, "{po}", {});
            }
            c += resolve_executable("curl"), "-fail", "-L", url, "-o", arch;
            log_info("downloading {}", url);
            c.run();
        }
        scope_exit se{[&] {
            fs::remove(arch);
        }};
        unpack(arch, wdir);
        se.disable();
        fs::remove(arch);
        {
            int ndir{}, nfile{};
            for (auto &&p : fs::directory_iterator{wdir}) {
                if (p.is_directory()) {
                    source_dir = p;
                    ++ndir;
                } else {
                    ++nfile;
                }
            }
            if (ndir == 1 && nfile == 0) {
                //source_dir = wdir / source_dir;
            } else {
                SW_UNIMPLEMENTED;
            }
        }
        if (source_dir.empty()) {
            throw std::runtime_error{"cannot detect source dir"};
        }
    }
    void make_archive() {
        {
            raw_command c;
            c.working_directory = source_dir;
            c += resolve_executable("tar"), "-c", "-f", arch;
            c += files;
            log_info("creating archive {}", arch);
            c.run();
        }
        fs::remove_all(source_dir);
    }

    void operator+=(auto &&v) {
        add(v);
    }
    void add(const file_regex_op &r) {
        switch (r.operation) {
        case file_regex_op::add: add(r.r); break;
        //case file_regex_op::remove: add(r.r); break;
        }
    }
    void add(const file_regex &r) {
        r(source_dir, [&](auto &&iter) {
            for (auto &&e : iter) {
                if (fs::is_regular_file(e)) {
                    //add(e);
                    files.insert(e.path().lexically_relative(source_dir));
                }
            }
        });
    }
};

template <typename Ops>
struct github_package {
    using type = Ops;

    std::string name;
    sw::package_path prefix;
    std::string user_repo;
    std::string version_tag;
    std::vector<sw::package_version> versions;
    file_regex_ops files;

    void download(auto &&swctx, auto &&tmpdir) {
        for (auto &&v : versions)
            download(swctx, tmpdir, v);
    }
    void download(auto &&swctx, auto &&tmpdir, auto &&version) {
        auto dst = swctx.mirror_fn(name, version);
        if (fs::exists(dst)) {
            return;
        }

        auto source = github(user_repo, version_tag);
        auto p = package{name, source, version};
        p.download(tmpdir);
        for (auto &&op : files) {
            p += op;
        }
        p.make_archive();

        fs::create_directories(dst.parent_path());
        fs::copy_file(p.arch, dst);
    }
};

namespace sw {

// common sources

template <typename Source>
struct package_description {
    string name;
};

struct repository {
    // add name?
    path dir;

    void init(auto &&swctx) {
        init1(swctx);
    }
    void init1(auto &&swctx) {
        auto sl = std::source_location::current();
        if (!fs::exists(sl.file_name())) {
            // unpack self
            SW_UNIMPLEMENTED;
        } else {
            // we are in dev mode, so ok
        }

        auto add_package = [](auto &&p) {
            p.make_archive();
        };

        file_regex_op c_files{".*\\.[hc]"_rr};
        file_regex_op c_files_no_recusive{".*\\.[hc]"_r};

        /*auto github_package = [&](auto &&name, auto &&user_repo, auto &&version_tag, auto &&version,
                                  const file_regex_ops &ops = {}) {
            auto source = github(user_repo, version_tag);
            auto p = package{name, source, version};
            download_package(p);
            for (auto &&op : ops) {
                p += op;
            }
            add_package(p);
            return p;
        };*/

        // demo = non official package provided by sw project
        // original authors should not use it when making an sw package
        #define DEMO_PREFIX "org.sw.demo."

        auto add_sources = [&](auto &&pkg) {
            add(swctx, pkg);
        };
        auto add_binary = [&](auto &&pkg) {
            using Builder = std::decay_t<decltype(pkg)>::type;

            for (auto &&v : pkg.versions) {
                auto fn = swctx.mirror_fn(pkg.name, v);
                auto root = swctx.pkg_root(pkg.name, v) / "sdir";
                if (!fs::exists(root)) {
                    unpack(fn, root);
                }
                //b.build();

                auto s = make_solution();
                input_with_settings is;
                is.ep.build = [](auto &&s) {
                    Builder b;
                    b.build(s);
                };
                is.ep.source_dir = root;
                auto dbs = default_build_settings();
                //dbs.build_type = build_type::debug{};
                is.settings.insert(dbs);
                s.add_input(is);
                s.load_inputs();
                command_line_parser cl;
                s.build(cl);
            }
        };

        github_package<zlib> zlib{"zlib", DEMO_PREFIX "zlib", "madler/zlib", "v{M}.{m}{po}", {
            "1.2.13"_ver,
            "1.3.0"_ver,
            "1.3.1"_ver,
        }};
        zlib.files.emplace_back(c_files_no_recusive);
        add_sources(zlib);
        add_binary(zlib);

        // check png 1.0.5
        github_package<png_1_0_5> png_1_0_5{"png", DEMO_PREFIX "png", "glennrp/libpng", "v{v}", {
            "1.0.5"_ver,
        }};
        png_1_0_5.files.emplace_back(c_files);
        add_sources(png_1_0_5);

        github_package<png> png{"png", DEMO_PREFIX "png", "glennrp/libpng", "v{v}", {
            "1.6.39"_ver,
            "1.6.40"_ver,
            "1.6.41"_ver,
            "1.6.42"_ver,
        }};
        png.files.emplace_back(c_files);
        add_sources(png);

        /*github_package<sw::self_build::sw_build> sw2{"sw2", "org.sw.sw", "SoftwareNetwork/sw2"};
        sw2.files.emplace_back("src/.*"_rr);
        add_sources(sw2);*/

        #undef DEMO_PREFIX
    }

private:
    void add(auto &&swctx, auto &&pkg) {
        pkg.download(swctx, swctx.temp_dir / "dl");
    }
};

}
