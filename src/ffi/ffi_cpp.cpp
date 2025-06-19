#include "ffi_cpp.h"
#include <stdexcept>

namespace tocin {
namespace ffi {

    void CppFFI::registerFunction(const std::string& name, CppFunction func) {
        if (name.empty()) {
            throw std::runtime_error("Cannot register function with empty name");
        }
        functions[name] = std::move(func);
    }

    ffi::FFIValue CppFFI::call(const std::string& functionName, const std::vector<ffi::FFIValue>& args) {
        auto it = functions.find(functionName);
        if (it == functions.end()) {
            throw std::runtime_error("Function not found: " + functionName);
        }
        try {
            return it->second(args);
        }
        catch (const std::exception& e) {
            throw std::runtime_error("Error calling " + functionName + ": " + e.what());
        }
    }

    bool CppFFI::hasFunction(const std::string& functionName) const {
        return functions.find(functionName) != functions.end();
    }

} // namespace ffi
} // namespace tocin