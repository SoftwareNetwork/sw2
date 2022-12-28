#pragma once

#include "helpers.h"

namespace sw {

struct native_sources_rule;
struct cl_exe_rule;
struct lib_exe_rule;
struct link_exe_rule;
struct gcc_compile_rule;
struct gcc_link_rule;
struct lib_ar_rule;

using rule_types = types<
        native_sources_rule, cl_exe_rule, lib_exe_rule, link_exe_rule, gcc_compile_rule, gcc_link_rule, lib_ar_rule>;

} // namespace sw
