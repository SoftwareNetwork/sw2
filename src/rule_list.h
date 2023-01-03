#pragma once

#include "helpers.h"

namespace sw {

struct native_sources_rule;
struct lib_exe_rule;
struct link_exe_rule;
struct gcc_compile_rule;
struct gcc_link_rule;
struct lib_ar_rule;

using rule_types = types<
        native_sources_rule, lib_exe_rule, link_exe_rule, gcc_compile_rule, gcc_link_rule, lib_ar_rule>;

using rule = rule_types::variant_type;

auto is_c_file(const path &fn) {
    static std::set<string> exts{".c", ".m"}; // with obj-c, separate call?
    return exts.contains(fn.extension().string());
}
auto is_cpp_file(const path &fn) {
    static std::set<string> exts{".cpp", ".cxx", ".mm"}; // with obj-c++, separate call?
    return exts.contains(fn.extension().string());
}

} // namespace sw
