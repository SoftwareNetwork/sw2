#pragma once

#include "string.h"

#include <stdexcept>

namespace sw {

struct unimplemented_exception : std::runtime_error {
    unimplemented_exception(std::source_location sl = std::source_location::current())
        : runtime_error{format("unimplemented: {}", sl)} {}
};
#define SW_UNIMPLEMENTED throw ::sw::unimplemented_exception{}

}
