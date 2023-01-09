#pragma once

#include "helpers.h"

namespace sw {

//struct files_target;
//struct rule_target;
struct native_target;
struct native_library_target;
struct native_shared_library_target;
struct native_static_library_target;
struct executable_target;
struct test_target;
//struct binary_target;
//struct binary_library_target;
//struct binary_target_msvc;

using target_type = types<
    //files_target, rule_target,
    native_target, native_library_target, native_shared_library_target,
    native_static_library_target, executable_target,
    test_target
    //, binary_target, binary_library_target, binary_target_msvc
>;

using target_ptr = target_type::variant_of_ptr_type;
using target_uptr = target_type::variant_of_uptr_type;

}
