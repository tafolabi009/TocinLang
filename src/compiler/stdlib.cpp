#include "tocin_stdlib.h"
#include <cmath>
#include <iostream>
#include <ostream>
#include <algorithm>
#include <sstream>
#include <random>

namespace tocin {
namespace compiler {

    void StdLib::registerFunctions(ffi::CppFFI& ffi) {
        ffi.registerFunction("print", [](const std::vector<ffi::FFIValue>& args) -> ffi::FFIValue {
            for (size_t i = 0; i < args.size(); ++i) {
                if (i > 0) std::cout << " ";
                auto t = args[i].getType();
                if (t == ffi::FFIValue::Type::INTEGER) std::cout << args[i].asInt64();
                else if (t == ffi::FFIValue::Type::FLOAT) std::cout << args[i].asDouble();
                else if (t == ffi::FFIValue::Type::BOOLEAN) std::cout << (args[i].asBoolean() ? "true" : "false");
                else if (t == ffi::FFIValue::Type::STRING) std::cout << args[i].asString();
                else std::cout << "unknown";
            }
            std::cout << std::endl;
            return ffi::FFIValue::createNull();
            });

        ffi.registerFunction("str_length", [](const std::vector<ffi::FFIValue>& args) -> ffi::FFIValue {
            if (args.size() != 1 || args[0].getType() != ffi::FFIValue::Type::STRING) {
                throw std::runtime_error("str_length expects one string");
            }
            return ffi::FFIValue(static_cast<int64_t>(args[0].asString().size()));
            });

        ffi.registerFunction("str_concat", [](const std::vector<ffi::FFIValue>& args) -> ffi::FFIValue {
            if (args.size() != 2 || args[0].getType() != ffi::FFIValue::Type::STRING ||
                args[1].getType() != ffi::FFIValue::Type::STRING) {
                throw std::runtime_error("str_concat expects two strings");
            }
            return ffi::FFIValue(args[0].asString() + args[1].asString());
            });

        ffi.registerFunction("str_slice", [](const std::vector<ffi::FFIValue>& args) -> ffi::FFIValue {
            if (args.size() != 3 || args[0].getType() != ffi::FFIValue::Type::STRING ||
                args[1].getType() != ffi::FFIValue::Type::INTEGER || args[2].getType() != ffi::FFIValue::Type::INTEGER) {
                throw std::runtime_error("str_slice expects string, start, end");
            }
            auto str = args[0].asString();
            int64_t start = args[1].asInt64();
            int64_t end = args[2].asInt64();
            if (start < 0 || end > static_cast<int64_t>(str.size()) || start > end) {
                return ffi::FFIValue(std::string(""));
            }
            return ffi::FFIValue(str.substr(start, end - start));
            });

        ffi.registerFunction("math_sqrt", [](const std::vector<ffi::FFIValue>& args) -> ffi::FFIValue {
            if (args.size() != 1 || args[0].getType() != ffi::FFIValue::Type::FLOAT) {
                throw std::runtime_error("sqrt expects one float");
            }
            return ffi::FFIValue(std::sqrt(args[0].asDouble()));
            });

        // Additional math functions
        ffi.registerFunction("math_pow", [](const std::vector<ffi::FFIValue>& args) -> ffi::FFIValue {
            if (args.size() != 2) {
                throw std::runtime_error("pow expects two numbers");
            }
            double base = (args[0].getType() == ffi::FFIValue::Type::FLOAT) ? args[0].asDouble() : static_cast<double>(args[0].asInt64());
            double exp = (args[1].getType() == ffi::FFIValue::Type::FLOAT) ? args[1].asDouble() : static_cast<double>(args[1].asInt64());
            return ffi::FFIValue(std::pow(base, exp));
            });

        ffi.registerFunction("math_sin", [](const std::vector<ffi::FFIValue>& args) -> ffi::FFIValue {
            if (args.size() != 1) throw std::runtime_error("sin expects one number");
            double val = (args[0].getType() == ffi::FFIValue::Type::FLOAT) ? args[0].asDouble() : static_cast<double>(args[0].asInt64());
            return ffi::FFIValue(std::sin(val));
            });

        ffi.registerFunction("math_cos", [](const std::vector<ffi::FFIValue>& args) -> ffi::FFIValue {
            if (args.size() != 1) throw std::runtime_error("cos expects one number");
            double val = (args[0].getType() == ffi::FFIValue::Type::FLOAT) ? args[0].asDouble() : static_cast<double>(args[0].asInt64());
            return ffi::FFIValue(std::cos(val));
            });

        ffi.registerFunction("math_tan", [](const std::vector<ffi::FFIValue>& args) -> ffi::FFIValue {
            if (args.size() != 1) throw std::runtime_error("tan expects one number");
            double val = (args[0].getType() == ffi::FFIValue::Type::FLOAT) ? args[0].asDouble() : static_cast<double>(args[0].asInt64());
            return ffi::FFIValue(std::tan(val));
            });

        ffi.registerFunction("math_abs", [](const std::vector<ffi::FFIValue>& args) -> ffi::FFIValue {
            if (args.size() != 1) throw std::runtime_error("abs expects one number");
            if (args[0].getType() == ffi::FFIValue::Type::FLOAT) {
                return ffi::FFIValue(std::abs(args[0].asDouble()));
            } else {
                return ffi::FFIValue(std::abs(args[0].asInt64()));
            }
            });

        ffi.registerFunction("math_floor", [](const std::vector<ffi::FFIValue>& args) -> ffi::FFIValue {
            if (args.size() != 1) throw std::runtime_error("floor expects one number");
            double val = (args[0].getType() == ffi::FFIValue::Type::FLOAT) ? args[0].asDouble() : static_cast<double>(args[0].asInt64());
            return ffi::FFIValue(std::floor(val));
            });

        ffi.registerFunction("math_ceil", [](const std::vector<ffi::FFIValue>& args) -> ffi::FFIValue {
            if (args.size() != 1) throw std::runtime_error("ceil expects one number");
            double val = (args[0].getType() == ffi::FFIValue::Type::FLOAT) ? args[0].asDouble() : static_cast<double>(args[0].asInt64());
            return ffi::FFIValue(std::ceil(val));
            });

        ffi.registerFunction("math_round", [](const std::vector<ffi::FFIValue>& args) -> ffi::FFIValue {
            if (args.size() != 1) throw std::runtime_error("round expects one number");
            double val = (args[0].getType() == ffi::FFIValue::Type::FLOAT) ? args[0].asDouble() : static_cast<double>(args[0].asInt64());
            return ffi::FFIValue(std::round(val));
            });

        ffi.registerFunction("math_log", [](const std::vector<ffi::FFIValue>& args) -> ffi::FFIValue {
            if (args.size() != 1) throw std::runtime_error("log expects one number");
            double val = (args[0].getType() == ffi::FFIValue::Type::FLOAT) ? args[0].asDouble() : static_cast<double>(args[0].asInt64());
            return ffi::FFIValue(std::log(val));
            });

        ffi.registerFunction("math_exp", [](const std::vector<ffi::FFIValue>& args) -> ffi::FFIValue {
            if (args.size() != 1) throw std::runtime_error("exp expects one number");
            double val = (args[0].getType() == ffi::FFIValue::Type::FLOAT) ? args[0].asDouble() : static_cast<double>(args[0].asInt64());
            return ffi::FFIValue(std::exp(val));
            });

        ffi.registerFunction("math_random", [](const std::vector<ffi::FFIValue>& args) -> ffi::FFIValue {
            static std::random_device rd;
            static std::mt19937 gen(rd());
            static std::uniform_real_distribution<> dis(0.0, 1.0);
            return ffi::FFIValue(dis(gen));
            });

        // Additional string functions
        ffi.registerFunction("str_to_upper", [](const std::vector<ffi::FFIValue>& args) -> ffi::FFIValue {
            if (args.size() != 1 || args[0].getType() != ffi::FFIValue::Type::STRING) {
                throw std::runtime_error("str_to_upper expects one string");
            }
            std::string str = args[0].asString();
            std::transform(str.begin(), str.end(), str.begin(), ::toupper);
            return ffi::FFIValue(str);
            });

        ffi.registerFunction("str_to_lower", [](const std::vector<ffi::FFIValue>& args) -> ffi::FFIValue {
            if (args.size() != 1 || args[0].getType() != ffi::FFIValue::Type::STRING) {
                throw std::runtime_error("str_to_lower expects one string");
            }
            std::string str = args[0].asString();
            std::transform(str.begin(), str.end(), str.begin(), ::tolower);
            return ffi::FFIValue(str);
            });

        ffi.registerFunction("str_trim", [](const std::vector<ffi::FFIValue>& args) -> ffi::FFIValue {
            if (args.size() != 1 || args[0].getType() != ffi::FFIValue::Type::STRING) {
                throw std::runtime_error("str_trim expects one string");
            }
            std::string str = args[0].asString();
            size_t start = str.find_first_not_of(" \t\n\r");
            if (start == std::string::npos) return ffi::FFIValue(std::string(""));
            size_t end = str.find_last_not_of(" \t\n\r");
            return ffi::FFIValue(str.substr(start, end - start + 1));
            });

        ffi.registerFunction("str_replace", [](const std::vector<ffi::FFIValue>& args) -> ffi::FFIValue {
            if (args.size() != 3 || args[0].getType() != ffi::FFIValue::Type::STRING ||
                args[1].getType() != ffi::FFIValue::Type::STRING || args[2].getType() != ffi::FFIValue::Type::STRING) {
                throw std::runtime_error("str_replace expects three strings");
            }
            std::string str = args[0].asString();
            std::string from = args[1].asString();
            std::string to = args[2].asString();
            size_t pos = 0;
            while ((pos = str.find(from, pos)) != std::string::npos) {
                str.replace(pos, from.length(), to);
                pos += to.length();
            }
            return ffi::FFIValue(str);
            });

        ffi.registerFunction("str_contains", [](const std::vector<ffi::FFIValue>& args) -> ffi::FFIValue {
            if (args.size() != 2 || args[0].getType() != ffi::FFIValue::Type::STRING ||
                args[1].getType() != ffi::FFIValue::Type::STRING) {
                throw std::runtime_error("str_contains expects two strings");
            }
            std::string str = args[0].asString();
            std::string substr = args[1].asString();
            return ffi::FFIValue(str.find(substr) != std::string::npos);
            });

        ffi.registerFunction("str_starts_with", [](const std::vector<ffi::FFIValue>& args) -> ffi::FFIValue {
            if (args.size() != 2 || args[0].getType() != ffi::FFIValue::Type::STRING ||
                args[1].getType() != ffi::FFIValue::Type::STRING) {
                throw std::runtime_error("str_starts_with expects two strings");
            }
            std::string str = args[0].asString();
            std::string prefix = args[1].asString();
            return ffi::FFIValue(str.substr(0, prefix.length()) == prefix);
            });

        ffi.registerFunction("str_ends_with", [](const std::vector<ffi::FFIValue>& args) -> ffi::FFIValue {
            if (args.size() != 2 || args[0].getType() != ffi::FFIValue::Type::STRING ||
                args[1].getType() != ffi::FFIValue::Type::STRING) {
                throw std::runtime_error("str_ends_with expects two strings");
            }
            std::string str = args[0].asString();
            std::string suffix = args[1].asString();
            if (suffix.length() > str.length()) return ffi::FFIValue(false);
            return ffi::FFIValue(str.substr(str.length() - suffix.length()) == suffix);
            });

        ffi.registerFunction("str_split", [](const std::vector<ffi::FFIValue>& args) -> ffi::FFIValue {
            if (args.size() != 2 || args[0].getType() != ffi::FFIValue::Type::STRING ||
                args[1].getType() != ffi::FFIValue::Type::STRING) {
                throw std::runtime_error("str_split expects two strings");
            }
            std::string str = args[0].asString();
            std::string delimiter = args[1].asString();
            std::vector<ffi::FFIValue> result;
            size_t pos = 0;
            size_t prev = 0;
            while ((pos = str.find(delimiter, prev)) != std::string::npos) {
                result.push_back(ffi::FFIValue(str.substr(prev, pos - prev)));
                prev = pos + delimiter.length();
            }
            result.push_back(ffi::FFIValue(str.substr(prev)));
            return ffi::FFIValue(result);
            });
    }

} // namespace compiler
} // namespace tocin