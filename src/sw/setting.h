#pragma once

#include "helpers/common.h"

namespace sw {

struct setting {
    virtual ~setting() {}
    virtual bool operator==(const setting &s) const = 0;
};

template <typename T>
struct setting_type : setting {
    T value;

    setting_type() = default;
    setting_type(auto && ... vals) : value{FWD(vals)...} {}
    bool operator==(const setting &s) const override {
        if (auto rhs = dynamic_cast<const setting_type<T>*>(&s)) {
            return value == rhs->value;
        }
        return false;
    }
    T *operator->() const { return &value; }
};

struct settings {
    struct container {
        std::unique_ptr<setting> value;

        bool operator==(const container &c) const {
            if (value && c.value) {
                return *value == *c.value;
            }
            return false;
        }
        bool operator<(const container & c) const {
            //if (value && c.value) {
                //return *value < *c.value;
            //}
            return value < c.value;
        }
    };
    struct assigner {
        settings &s;
        std::string key;

        template <typename T>
        void operator=(T &&v) {
            std::unique_ptr<setting> u = std::make_unique<setting_type<T>>(v);
            s.value.emplace(key, std::move(u));
        }
    };

    std::map<std::string, container> value;

    bool operator<(const settings &s) const {
        return value < s.value;
    }
    bool operator==(const settings &s) const {
        return value == s.value;
    }
    auto operator[](auto &&s) {
        return assigner{*this,s};
    }
};

}
