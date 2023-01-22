#pragma once

#include "helpers/common.h"

namespace sw {

struct native_target;
struct native_library_target;
struct native_shared_library_target;
struct native_static_library_target;
struct executable_target;
struct test_target;

using target_type = types<
    native_target, native_library_target, native_shared_library_target,
    native_static_library_target, executable_target,
    test_target
>;

using target_ptr = target_type::variant_of_ptr_type;
using target_uptr = target_type::variant_of_uptr_type;

}
