#pragma once

#include "crypto/sha3.h"

#include <string>

namespace sw {

auto bytes_to_string(auto &&bytes) {
    std::string s;
    s.reserve(bytes.size() * 2);
    for (auto &&b : bytes) {
        constexpr auto alph = "0123456789abcdef";
        s += alph[b >> 4];
        s += alph[b & 0xF];
    }
    return s;
}

template <typename T>
auto digest(auto &&data) {
    T t;
    t.update(data);
    return bytes_to_string(t.digest());
}

}
