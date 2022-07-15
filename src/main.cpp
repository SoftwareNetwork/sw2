#include "storage.h"

#include <set>

void add_file_to_storage(auto &&s, auto &&f) {
}

void add_transform_to_storage(auto &&s, auto &&f) {
    add_file_to_storage(s,f);
}

struct files_target {
    using files_t = std::set<path>;
    files_t files; // unordered?

    void op_from_iterator(auto &&iter) {
        for (auto &&e : iter) {
            if (fs::is_regular_file(e)) {
                files.insert(e);
            }
        }
    }
    void add_from_iterator(auto &&iter) {
        for (auto &&e : iter) {
            if (fs::is_regular_file(e)) {
                files.insert(e);
            }
        }
    }
    template <bool recursive = false>
    void add_from_directory(auto &&dir) {
        if constexpr (recursive) {
            add_from_iterator(fs::recursive_directory_iterator{dir});
        } else {
            add_from_iterator(fs::directory_iterator{dir});
        }
    }
    void operator+=(auto &&iter) {
        add_from_iterator(iter);
    }
    void operator-=(auto &&iter) {
        add_from_iterator(iter);
    }
};
auto operator""_r(const char *dir, size_t) {
    return fs::directory_iterator{dir};
}
auto operator""_rr(const char *dir, size_t) {
    return fs::recursive_directory_iterator{dir};
}

auto build_some_package() {
    files_target tgt;
    tgt += "d:/dev/cppan2/client4/src"_rr;
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
			//handle.size();
		}
}
