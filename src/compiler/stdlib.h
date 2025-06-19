#pragma once

#include <string>
#include <vector>
#include "../ffi/ffi_cpp.h"

namespace tocin {
namespace compiler {

class StdLib {
public:
    static void registerFunctions(tocin::ffi::CppFFI& ffi);
};

} // namespace compiler
} // namespace tocin