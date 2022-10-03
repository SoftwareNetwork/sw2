#pragma once

#include "helpers.h"

namespace sw {

struct istring : string {
    using string::string;
    using string::operator=;

     //std::strong_ordering operator<=>(const istring &rhs) const { return stricmp(data(), rhs.data()); }
};

struct package_name {
    //std::vector<istring> elements;
    std::vector<string> elements;

    package_name() {
    }
    package_name(const string &s) : elements{s} {
    }
};

struct package_version {
    struct number_version {
        using numbers = std::vector<int>;
        numbers elements;
        string extra;
    };
    std::variant<number_version, string> version;

    package_version() : version{number_version{{0,0,1}}} {
    }
    package_version(const string &s) {
    }
};

struct package_id {
    package_name name;
    package_version version;
};

}
