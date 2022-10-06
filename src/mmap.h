#pragma once

#include "win32.h"

#ifdef _WIN32
#else
#include <sys/mman.h>
#endif

namespace sw {

template <typename T = uint8_t>
struct mmap_file {
    struct ro{
        static inline constexpr auto access = GENERIC_READ;
        static inline constexpr auto share_mode = FILE_SHARE_READ;
        static inline constexpr auto disposition = OPEN_EXISTING;
        static inline constexpr auto page_mode = PAGE_READONLY;
        static inline constexpr auto map_mode = FILE_MAP_READ;
    };
    struct rw {
        static inline constexpr auto access = GENERIC_READ | GENERIC_WRITE;
        static inline constexpr auto share_mode = FILE_SHARE_READ | FILE_SHARE_WRITE;
        static inline constexpr auto disposition = OPEN_ALWAYS;
        static inline constexpr auto page_mode = PAGE_READWRITE;
        static inline constexpr auto map_mode = FILE_MAP_READ | FILE_MAP_WRITE;
    };

    using size_type = uint64_t;

#ifdef _WIN32
    win32::handle f, m;
#else
    int fd;
#endif
    T *p{nullptr};
    size_type sz;

    mmap_file(const fs::path &fn) {
        open(fn, ro{});
    }
    mmap_file(const fs::path &fn, rw v) {
        open(fn, v);
    }
    void open(const path &fn, auto mode) {
        sz = !fs::exists(fn) ? 0 : fs::file_size(fn) / sizeof(T);
        if (sz == 0) {
            return;
        }
#ifdef _WIN32
        f = win32::handle{CreateFileW(fn.wstring().c_str(), mode.access, mode.share_mode, 0, mode.disposition, FILE_ATTRIBUTE_NORMAL, 0),
            [&] { throw std::runtime_error{"cannot open file: " + fn.string()}; }};
        m = win32::handle{CreateFileMappingW(f, 0, mode.page_mode, 0, 0, 0),
            [&] { throw std::runtime_error{"cannot create file mapping"}; }};
        p = (T *)MapViewOfFile(m, mode.map_mode, 0, 0, 0);
        if (!p) {
            throw std::runtime_error{"cannot map file"};
        }
#else
        fd = open(fn.string().c_str(), O_RDONLY);
        if (fd == -1) {
            throw std::runtime_error{"cannot open file: " + fn.string()};
        }
        p = (T *)mmap(0, sz, PROT_READ, MAP_PRIVATE, fd, 0);
        if (p == MAP_FAILED) {
            close(fd);
            throw std::runtime_error{"cannot create file mapping"};
        }
#endif
    }
    ~mmap_file() {
#ifdef _WIN32
        if (p) {
            UnmapViewOfFile(p);
        }
#else
        close(fd);
#endif
    }
    auto &operator[](int i) { return p[i]; }
    const auto &operator[](int i) const { return p[i]; }
    bool eof(size_type pos) const { return pos >= sz; }
    operator T*() const { return p; }
    template <typename U>
    operator U*() const { return (U*)p; }

    auto begin() const { return p; }
    auto end() const { return p+sz; }
};

} // namespace primitives::templates2
