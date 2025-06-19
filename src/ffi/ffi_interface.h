#ifndef FFI_INTERFACE_H
#define FFI_INTERFACE_H

#include "ffi_value.h"
#include <string>
#include <vector>

namespace tocin {
namespace ffi {

    class FFIInterface {
    public:
        virtual ~FFIInterface() = default;
        virtual FFIValue call(const std::string& functionName, const std::vector<FFIValue>& args) = 0;
        virtual bool hasFunction(const std::string& functionName) const = 0;
    };

} // namespace ffi
} // namespace tocin

#endif // FFI_INTERFACE_H