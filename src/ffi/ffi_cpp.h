#ifndef FFI_CPP_H
#define FFI_CPP_H

#include "ffi_interface.h"
#include "ffi_value.h"
#include <functional>
#include <unordered_map>

namespace tocin {
namespace ffi {

    class CppFFI : public FFIInterface {
    public:
        using CppFunction = std::function<FFIValue(const std::vector<FFIValue>&)>;

        CppFFI() = default;
        ~CppFFI() override = default;

        void registerFunction(const std::string& name, CppFunction func);
        FFIValue call(const std::string& functionName, const std::vector<FFIValue>& args) override;
        bool hasFunction(const std::string& functionName) const override;

    private:
        std::unordered_map<std::string, CppFunction> functions;
    };

} // namespace ffi
} // namespace tocin

#endif // FFI_CPP_H