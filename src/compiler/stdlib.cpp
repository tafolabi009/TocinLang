#include "stdlib.h"
#include <cmath>
#include <iostream>
#include <ostream>

namespace tocin {
namespace compiler {

    void StdLib::registerFunctions(tocin::ffi::CppFFI& ffi) {
        ffi.registerFunction("print", [](const std::vector<ffi::FFIValue>& args) -> ffi::FFIValue {
            for (size_t i = 0; i < args.size(); ++i) {
                if (i > 0) std::cout << " ";
                switch (args[i].getType()) {
                case ffi::FFIValueType::Int64: std::cout << args[i].getInt64(); break;
                case ffi::FFIValueType::Double: std::cout << args[i].getDouble(); break;
                case ffi::FFIValueType::Bool: std::cout << (args[i].getBool() ? "true" : "false"); break;
                case ffi::FFIValueType::String: std::cout << args[i].getString(); break;
                default: std::cout << "unknown";
                }
            }
            std::cout << std::endl;
            return ffi::FFIValue();
            });

        ffi.registerFunction("str_length", [](const std::vector<ffi::FFIValue>& args) -> ffi::FFIValue {
            if (args.size() != 1 || args[0].getType() != ffi::FFIValueType::String) {
                throw std::runtime_error("str_length expects one string");
            }
            return ffi::FFIValue(static_cast<int64_t>(args[0].getString().size()));
            });

        ffi.registerFunction("str_concat", [](const std::vector<ffi::FFIValue>& args) -> ffi::FFIValue {
            if (args.size() != 2 || args[0].getType() != ffi::FFIValueType::String ||
                args[1].getType() != ffi::FFIValueType::String) {
                throw std::runtime_error("str_concat expects two strings");
            }
            return ffi::FFIValue(args[0].getString() + args[1].getString());
            });

        ffi.registerFunction("str_slice", [](const std::vector<ffi::FFIValue>& args) -> ffi::FFIValue {
            if (args.size() != 3 || args[0].getType() != ffi::FFIValueType::String ||
                args[1].getType() != ffi::FFIValueType::Int64 || args[2].getType() != ffi::FFIValueType::Int64) {
                throw std::runtime_error("str_slice expects string, start, end");
            }
            auto str = args[0].getString();
            int64_t start = args[1].getInt64();
            int64_t end = args[2].getInt64();
            if (start < 0 || end > static_cast<int64_t>(str.size()) || start > end) {
                return ffi::FFIValue("");
            }
            return ffi::FFIValue(str.substr(start, end - start));
            });

        ffi.registerFunction("math_sqrt", [](const std::vector<ffi::FFIValue>& args) -> ffi::FFIValue {
            if (args.size() != 1 || args[0].getType() != ffi::FFIValueType::Double) {
                throw std::runtime_error("sqrt expects one float");
            }
            return ffi::FFIValue(std::sqrt(args[0].getDouble()));
            });
    }

} // namespace compiler
} // namespace tocin