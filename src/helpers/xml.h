#pragma once

#include "common.h"

namespace sw {

template <typename T>
struct xml_tag {
    T *parent{};
    string name;
    int depth{};
    //
    string s;
    std::map<string,string> attributes;

    ~xml_tag() {
        if (!parent) {
            return;
        }
        string a;
        for (auto &&[k,v] : attributes) {
            a += format(" {}=\"{}\"", k, v);
        }
        if (!s.empty()) {
            parent->add_line(format("<{}{}>", name, a));
            parent->s += s;
            parent->add_line(format("</{}>", name));
        } else {
            parent->add_line(format("<{}{} />", name, a));
        }
    }
    void add_line(auto &&l) {
        s += string(depth, '\t');
        s += l;
        s += "\n";
    }
    auto tag(auto &&obj, auto &&name) {
        return xml_tag<xml_tag<T>>{obj, name, depth+1};
    }
    auto tag(auto &&name) {
        return tag(this, name);
    }
    string &operator[](auto &&v) {
        return attributes[v];
    }
};

struct xml_emitter : xml_tag<xml_emitter> {
    xml_emitter() {
        add_line("<?xml version=\"1.0\"?>");
    }
};

} // namespace sw
