// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2022 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include "win32.h"

#ifdef _WIN32
#else
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace sw {

template <typename T = uint8_t>
struct mmap_file {
#ifdef _WIN32
    struct ro{
        static inline constexpr auto access = GENERIC_READ;
        static inline constexpr auto share_mode = FILE_SHARE_READ;
        static inline constexpr auto disposition = OPEN_EXISTING;
        static inline constexpr auto page_mode = PAGE_READONLY;
        static inline constexpr auto map_mode = FILE_MAP_READ;
    };
    struct rw {
        static inline constexpr auto access = GENERIC_READ | GENERIC_WRITE;
        static inline constexpr auto share_mode = FILE_SHARE_READ;// | FILE_SHARE_WRITE;
        static inline constexpr auto disposition = OPEN_ALWAYS;
        static inline constexpr auto page_mode = PAGE_READWRITE;
        static inline constexpr auto map_mode = FILE_MAP_READ | FILE_MAP_WRITE;
    };
#else
    struct ro{
        static inline constexpr auto open_mode = O_RDONLY;
        static inline constexpr auto prot_mode = PROT_READ;
    };
    struct rw{
        static inline constexpr auto open_mode = O_RDWR;
        static inline constexpr auto prot_mode = PROT_READ | PROT_WRITE;
    };
#endif

    using size_type = uint64_t;

    path fn;
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
        this->fn = fn;
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
        fd = ::open(fn.string().c_str(), mode.open_mode);
        if (fd == -1) {
            throw std::runtime_error{"cannot open file: " + fn.string()};
        }
        p = (T *)mmap(0, sz, mode.prot_mode, MAP_PRIVATE, fd, 0);
        if (p == MAP_FAILED) {
            ::close(fd);
            throw std::runtime_error{"cannot create file mapping"};
        }
#endif
    }
    void close() {
#ifdef _WIN32
        if (p) {
            UnmapViewOfFile(p);
        }
        f.reset();
        m.reset();
#else
        ::close(fd);
#endif
    }
    ~mmap_file() {
        close();
    }
    auto &operator[](int i) { return p[i]; }
    const auto &operator[](int i) const { return p[i]; }
    bool eof(size_type pos) const { return pos >= sz; }
    operator T*() const { return p; }
    template <typename U>
    operator U*() const { return (U*)p; }

    auto begin() const { return p; }
    auto end() const { return p+sz; }

    T *alloc(size_type sz) {
        close();
        auto oldsz = this->sz;
        if (!fs::exists(fn)) {
            std::ofstream{fn};
        }
        fs::resize_file(fn, oldsz ? this->sz * 2 + sz : sz * 2);
        open(fn, rw{});
        return p + oldsz;
    }

    struct stream {
        mmap_file &m;
        size_type offset{0};

        auto size() const { return m.sz; }
        bool has_room(auto sz) const { return offset + sz <= m.sz; }
        explicit operator bool() const { return offset != -1; }

        auto write_record(size_type sz) {
            if (!has_room(sz)) {
                m.alloc(sz);
            }
            *this << sz;
            auto oldoff = offset;
            offset += sz;
            return stream{m,oldoff};
        }
        auto read_record() {
            size_type sz;
            if (!has_room(sizeof(sz))) {
                return stream{m, (size_type)-1};
            }
            *this >> sz;
            if (sz == 0) {
                offset -= sizeof(sz);
                return stream{m, (size_type)-1};
            }
            if (!has_room(sz)) {
                return stream{m, (size_type)-1};
            }
            auto oldoff = offset;
            offset += sz;
            return stream{m,oldoff};
        }

        template <typename U>
        stream &operator>>(U &v) {
            if (!has_room(sizeof(U))) {
                throw std::runtime_error{"no more data"};
            }
            v = *(U*)(m.p + offset);
            offset += sizeof(U);
            return *this;
        }
        template <typename U>
        stream &operator<<(const U &v) {
            if (!has_room(sizeof(U))) {
                throw std::runtime_error{"no more room"};
            }
            *(U *)(m.p + offset) = v;
            offset += sizeof(U);
            return *this;
        }
    };
    auto get_stream() {
        return stream{*this};
    }
};

} // namespace primitives::templates2
