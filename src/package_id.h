// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2022 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include "package.h"
#include "os.h"

namespace sw {

struct package_id {
    package_name name;
    build_settings settings;

    auto hash() const {
        return name.hash() ^ settings.hash();
    }

    //auto operator<=>(const package_id &) const = default;
    bool operator<(const package_id &rhs) const {
        return std::tie(name,settings) < std::tie(rhs.name,rhs.settings);
    }
};

struct unresolved_package_id {
};

}
