#pragma once

#include "crypto/sha3.h"

#include <string>

namespace sw {

template <typename T>
std::string digest(auto &&data) {
    T t;
    t.update(data);
    auto r = t.digest();
    std::string s(r.begin(), r.end());
    return s;
}

}
