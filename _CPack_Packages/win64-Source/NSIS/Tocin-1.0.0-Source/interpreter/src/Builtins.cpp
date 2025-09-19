#include "Builtins.h"
#include <iostream>
#include <cmath>
#include <random>
#include <chrono>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <filesystem>
#include <cstdlib>

namespace interpreter {

    void Builtins::registerBuiltins(std::unordered_map<std::string, BuiltinFunction>& builtins) {
        // String operations
        builtins["print"] = print;
        builtins["str_length"] = strLength;
        builtins["str_concat"] = strConcat;
        builtins["str_slice"] = strSlice;
        builtins["str_split"] = strSplit;
        builtins["str_join"] = strJoin;
        builtins["str_trim"] = strTrim;
        builtins["str_to_upper"] = strToUpper;
        builtins["str_to_lower"] = strToLower;
        builtins["str_replace"] = strReplace;
        builtins["str_contains"] = strContains;
        builtins["str_starts_with"] = strStartsWith;
        builtins["str_ends_with"] = strEndsWith;

        // Math operations
        builtins["math_sqrt"] = mathSqrt;
        builtins["math_pow"] = mathPow;
        builtins["math_sin"] = mathSin;
        builtins["math_cos"] = mathCos;
        builtins["math_tan"] = mathTan;
        builtins["math_log"] = mathLog;
        builtins["math_exp"] = mathExp;
        builtins["math_abs"] = mathAbs;
        builtins["math_floor"] = mathFloor;
        builtins["math_ceil"] = mathCeil;
        builtins["math_round"] = mathRound;
        builtins["math_random"] = mathRandom;

        // Array operations
        builtins["array_length"] = arrayLength;
        builtins["array_push"] = arrayPush;
        builtins["array_pop"] = arrayPop;
        builtins["array_shift"] = arrayShift;
        builtins["array_unshift"] = arrayUnshift;
        builtins["array_slice"] = arraySlice;
        builtins["array_concat"] = arrayConcat;
        builtins["array_reverse"] = arrayReverse;
        builtins["array_sort"] = arraySort;
        builtins["array_filter"] = arrayFilter;
        builtins["array_map"] = arrayMap;
        builtins["array_reduce"] = arrayReduce;

        // Dictionary operations
        builtins["dict_keys"] = dictKeys;
        builtins["dict_values"] = dictValues;
        builtins["dict_has_key"] = dictHasKey;
        builtins["dict_get"] = dictGet;
        builtins["dict_set"] = dictSet;
        builtins["dict_delete"] = dictDelete;
        builtins["dict_merge"] = dictMerge;

        // System operations
        builtins["time_now"] = timeNow;
        builtins["time_sleep"] = timeSleep;
        builtins["system_exit"] = systemExit;
        builtins["system_env"] = systemEnv;
        builtins["system_cwd"] = systemCwd;
        builtins["system_exec"] = systemExec;

        // Type conversion
        builtins["to_int"] = toInt;
        builtins["to_float"] = toFloat;
        builtins["to_string"] = toString;
        builtins["to_bool"] = toBool;
        builtins["to_array"] = toArray;
        builtins["to_dict"] = toDict;
    }

    Value Builtins::print(const std::vector<Value>& args) {
        for (size_t i = 0; i < args.size(); ++i) {
            if (i > 0) std::cout << " ";
            std::visit([](const auto& v) { std::cout << v; }, args[i]);
        }
        std::cout << std::endl;
        return Value();
    }

    Value Builtins::strLength(const std::vector<Value>& args) {
        if (args.size() != 1 || !std::holds_alternative<std::string>(args[0])) {
            throw std::runtime_error("str_length expects one string");
        }
        return Value(static_cast<int64_t>(std::get<std::string>(args[0]).size()));
    }

    Value Builtins::strConcat(const std::vector<Value>& args) {
        if (args.size() != 2 || !std::holds_alternative<std::string>(args[0]) || !std::holds_alternative<std::string>(args[1])) {
            throw std::runtime_error("str_concat expects two strings");
        }
        return Value(std::get<std::string>(args[0]) + std::get<std::string>(args[1]));
    }

    Value Builtins::strSlice(const std::vector<Value>& args) {
        if (args.size() != 3 || !std::holds_alternative<std::string>(args[0]) || !std::holds_alternative<int64_t>(args[1]) || !std::holds_alternative<int64_t>(args[2])) {
            throw std::runtime_error("str_slice expects string, start, end");
        }
        auto str = std::get<std::string>(args[0]);
        int64_t start = std::get<int64_t>(args[1]);
        int64_t end = std::get<int64_t>(args[2]);
        if (start < 0 || end > static_cast<int64_t>(str.size()) || start > end) {
            return Value("");
        }
        return Value(str.substr(start, end - start));
    }

    Value Builtins::mathSqrt(const std::vector<Value>& args) {
        if (args.size() != 1 || !std::holds_alternative<double>(args[0])) {
            throw std::runtime_error("sqrt expects one float");
        }
        return Value(std::sqrt(std::get<double>(args[0])));
    }

    Value Builtins::strSplit(const std::vector<Value>& args) {
        if (args.size() != 2 || !std::holds_alternative<std::string>(args[0]) || !std::holds_alternative<std::string>(args[1])) {
            throw std::runtime_error("str_split expects string and delimiter");
        }
        std::string str = std::get<std::string>(args[0]);
        std::string delim = std::get<std::string>(args[1]);
        std::vector<Value> result;
        size_t pos = 0;
        std::string token;
        while ((pos = str.find(delim)) != std::string::npos) {
            token = str.substr(0, pos);
            result.push_back(Value(token));
            str.erase(0, pos + delim.length());
        }
        result.push_back(Value(str));
        return Value(result);
    }

    Value Builtins::strJoin(const std::vector<Value>& args) {
        if (args.size() != 2 || !std::holds_alternative<std::vector<Value>>(args[0]) || !std::holds_alternative<std::string>(args[1])) {
            throw std::runtime_error("str_join expects array and delimiter");
        }
        std::vector<Value> arr = std::get<std::vector<Value>>(args[0]);
        std::string delim = std::get<std::string>(args[1]);
        std::stringstream ss;
        for (size_t i = 0; i < arr.size(); ++i) {
            if (i > 0) ss << delim;
            ss << std::get<std::string>(toString({arr[i]}));
        }
        return Value(ss.str());
    }

    Value Builtins::strTrim(const std::vector<Value>& args) {
        if (args.size() != 1 || !std::holds_alternative<std::string>(args[0])) {
            throw std::runtime_error("str_trim expects one string");
        }
        std::string str = std::get<std::string>(args[0]);
        str.erase(0, str.find_first_not_of(" \t\n\r\f\v"));
        str.erase(str.find_last_not_of(" \t\n\r\f\v") + 1);
        return Value(str);
    }

    Value Builtins::strToUpper(const std::vector<Value>& args) {
        if (args.size() != 1 || !std::holds_alternative<std::string>(args[0])) {
            throw std::runtime_error("str_to_upper expects one string");
        }
        std::string str = std::get<std::string>(args[0]);
        std::transform(str.begin(), str.end(), str.begin(), ::toupper);
        return Value(str);
    }

    Value Builtins::strToLower(const std::vector<Value>& args) {
        if (args.size() != 1 || !std::holds_alternative<std::string>(args[0])) {
            throw std::runtime_error("str_to_lower expects one string");
        }
        std::string str = std::get<std::string>(args[0]);
        std::transform(str.begin(), str.end(), str.begin(), ::tolower);
        return Value(str);
    }

    Value Builtins::strReplace(const std::vector<Value>& args) {
        if (args.size() != 3 || !std::holds_alternative<std::string>(args[0]) || 
            !std::holds_alternative<std::string>(args[1]) || !std::holds_alternative<std::string>(args[2])) {
            throw std::runtime_error("str_replace expects string, pattern, and replacement");
        }
        std::string str = std::get<std::string>(args[0]);
        std::string pattern = std::get<std::string>(args[1]);
        std::string replacement = std::get<std::string>(args[2]);
        size_t pos = 0;
        while ((pos = str.find(pattern, pos)) != std::string::npos) {
            str.replace(pos, pattern.length(), replacement);
            pos += replacement.length();
        }
        return Value(str);
    }

    Value Builtins::strContains(const std::vector<Value>& args) {
        if (args.size() != 2 || !std::holds_alternative<std::string>(args[0]) || !std::holds_alternative<std::string>(args[1])) {
            throw std::runtime_error("str_contains expects two strings");
        }
        std::string str = std::get<std::string>(args[0]);
        std::string pattern = std::get<std::string>(args[1]);
        return Value(str.find(pattern) != std::string::npos);
    }

    Value Builtins::strStartsWith(const std::vector<Value>& args) {
        if (args.size() != 2 || !std::holds_alternative<std::string>(args[0]) || !std::holds_alternative<std::string>(args[1])) {
            throw std::runtime_error("str_starts_with expects two strings");
        }
        std::string str = std::get<std::string>(args[0]);
        std::string prefix = std::get<std::string>(args[1]);
        return Value(str.substr(0, prefix.length()) == prefix);
    }

    Value Builtins::strEndsWith(const std::vector<Value>& args) {
        if (args.size() != 2 || !std::holds_alternative<std::string>(args[0]) || !std::holds_alternative<std::string>(args[1])) {
            throw std::runtime_error("str_ends_with expects two strings");
        }
        std::string str = std::get<std::string>(args[0]);
        std::string suffix = std::get<std::string>(args[1]);
        if (str.length() < suffix.length()) return Value(false);
        return Value(str.substr(str.length() - suffix.length()) == suffix);
    }

    Value Builtins::mathPow(const std::vector<Value>& args) {
        if (args.size() != 2 || !std::holds_alternative<double>(args[0]) || !std::holds_alternative<double>(args[1])) {
            throw std::runtime_error("math_pow expects two numbers");
        }
        return Value(std::pow(std::get<double>(args[0]), std::get<double>(args[1])));
    }

    Value Builtins::mathSin(const std::vector<Value>& args) {
        if (args.size() != 1 || !std::holds_alternative<double>(args[0])) {
            throw std::runtime_error("math_sin expects one number");
        }
        return Value(std::sin(std::get<double>(args[0])));
    }

    Value Builtins::mathCos(const std::vector<Value>& args) {
        if (args.size() != 1 || !std::holds_alternative<double>(args[0])) {
            throw std::runtime_error("math_cos expects one number");
        }
        return Value(std::cos(std::get<double>(args[0])));
    }

    Value Builtins::mathTan(const std::vector<Value>& args) {
        if (args.size() != 1 || !std::holds_alternative<double>(args[0])) {
            throw std::runtime_error("math_tan expects one number");
        }
        return Value(std::tan(std::get<double>(args[0])));
    }

    Value Builtins::mathLog(const std::vector<Value>& args) {
        if (args.size() != 1 || !std::holds_alternative<double>(args[0])) {
            throw std::runtime_error("math_log expects one number");
        }
        return Value(std::log(std::get<double>(args[0])));
    }

    Value Builtins::mathExp(const std::vector<Value>& args) {
        if (args.size() != 1 || !std::holds_alternative<double>(args[0])) {
            throw std::runtime_error("math_exp expects one number");
        }
        return Value(std::exp(std::get<double>(args[0])));
    }

    Value Builtins::mathAbs(const std::vector<Value>& args) {
        if (args.size() != 1 || !std::holds_alternative<double>(args[0])) {
            throw std::runtime_error("math_abs expects one number");
        }
        return Value(std::abs(std::get<double>(args[0])));
    }

    Value Builtins::mathFloor(const std::vector<Value>& args) {
        if (args.size() != 1 || !std::holds_alternative<double>(args[0])) {
            throw std::runtime_error("math_floor expects one number");
        }
        return Value(std::floor(std::get<double>(args[0])));
    }

    Value Builtins::mathCeil(const std::vector<Value>& args) {
        if (args.size() != 1 || !std::holds_alternative<double>(args[0])) {
            throw std::runtime_error("math_ceil expects one number");
        }
        return Value(std::ceil(std::get<double>(args[0])));
    }

    Value Builtins::mathRound(const std::vector<Value>& args) {
        if (args.size() != 1 || !std::holds_alternative<double>(args[0])) {
            throw std::runtime_error("math_round expects one number");
        }
        return Value(std::round(std::get<double>(args[0])));
    }

    Value Builtins::mathRandom(const std::vector<Value>& args) {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_real_distribution<> dis(0.0, 1.0);
        return Value(dis(gen));
    }

    Value Builtins::arrayLength(const std::vector<Value>& args) {
        if (args.size() != 1 || !std::holds_alternative<std::vector<Value>>(args[0])) {
            throw std::runtime_error("array_length expects one array");
        }
        return Value(static_cast<int64_t>(std::get<std::vector<Value>>(args[0]).size()));
    }

    Value Builtins::arrayPush(const std::vector<Value>& args) {
        if (args.size() < 2 || !std::holds_alternative<std::vector<Value>>(args[0])) {
            throw std::runtime_error("array_push expects array and at least one value");
        }
        std::vector<Value> arr = std::get<std::vector<Value>>(args[0]);
        for (size_t i = 1; i < args.size(); ++i) {
            arr.push_back(args[i]);
        }
        return Value(arr);
    }

    Value Builtins::arrayPop(const std::vector<Value>& args) {
        if (args.size() != 1 || !std::holds_alternative<std::vector<Value>>(args[0])) {
            throw std::runtime_error("array_pop expects one array");
        }
        std::vector<Value> arr = std::get<std::vector<Value>>(args[0]);
        if (arr.empty()) {
            throw std::runtime_error("Cannot pop from empty array");
        }
        Value result = arr.back();
        arr.pop_back();
        return result;
    }

    Value Builtins::dictKeys(const std::vector<Value>& args) {
        if (args.size() != 1 || !std::holds_alternative<std::unordered_map<std::string, Value>>(args[0])) {
            throw std::runtime_error("dict_keys expects one dictionary");
        }
        auto dict = std::get<std::unordered_map<std::string, Value>>(args[0]);
        std::vector<Value> keys;
        for (const auto& pair : dict) {
            keys.push_back(Value(pair.first));
        }
        return Value(keys);
    }

    Value Builtins::dictValues(const std::vector<Value>& args) {
        if (args.size() != 1 || !std::holds_alternative<std::unordered_map<std::string, Value>>(args[0])) {
            throw std::runtime_error("dict_values expects one dictionary");
        }
        auto dict = std::get<std::unordered_map<std::string, Value>>(args[0]);
        std::vector<Value> values;
        for (const auto& pair : dict) {
            values.push_back(pair.second);
        }
        return Value(values);
    }

    Value Builtins::dictHasKey(const std::vector<Value>& args) {
        if (args.size() != 2 || !std::holds_alternative<std::unordered_map<std::string, Value>>(args[0]) || 
            !std::holds_alternative<std::string>(args[1])) {
            throw std::runtime_error("dict_has_key expects dictionary and key");
        }
        auto dict = std::get<std::unordered_map<std::string, Value>>(args[0]);
        std::string key = std::get<std::string>(args[1]);
        return Value(dict.find(key) != dict.end());
    }

    Value Builtins::dictGet(const std::vector<Value>& args) {
        if (args.size() != 2 || !std::holds_alternative<std::unordered_map<std::string, Value>>(args[0]) || 
            !std::holds_alternative<std::string>(args[1])) {
            throw std::runtime_error("dict_get expects dictionary and key");
        }
        auto dict = std::get<std::unordered_map<std::string, Value>>(args[0]);
        std::string key = std::get<std::string>(args[1]);
        auto it = dict.find(key);
        return it != dict.end() ? it->second : Value();
    }

    Value Builtins::dictSet(const std::vector<Value>& args) {
        if (args.size() != 3 || !std::holds_alternative<std::unordered_map<std::string, Value>>(args[0]) || 
            !std::holds_alternative<std::string>(args[1]) || !std::holds_alternative<Value>(args[2])) {
            throw std::runtime_error("dict_set expects dictionary, key, and value");
        }
        auto dict = std::get<std::unordered_map<std::string, Value>>(args[0]);
        std::string key = std::get<std::string>(args[1]);
        Value value = std::get<Value>(args[2]);
        dict[key] = value;
        return Value();
    }

    Value Builtins::dictDelete(const std::vector<Value>& args) {
        if (args.size() != 2 || !std::holds_alternative<std::unordered_map<std::string, Value>>(args[0]) || 
            !std::holds_alternative<std::string>(args[1])) {
            throw std::runtime_error("dict_delete expects dictionary and key");
        }
        auto dict = std::get<std::unordered_map<std::string, Value>>(args[0]);
        std::string key = std::get<std::string>(args[1]);
        dict.erase(key);
        return Value();
    }

    Value Builtins::dictMerge(const std::vector<Value>& args) {
        if (args.size() != 2 || !std::holds_alternative<std::unordered_map<std::string, Value>>(args[0]) || 
            !std::holds_alternative<std::unordered_map<std::string, Value>>(args[1])) {
            throw std::runtime_error("dict_merge expects two dictionaries");
        }
        auto dict1 = std::get<std::unordered_map<std::string, Value>>(args[0]);
        auto dict2 = std::get<std::unordered_map<std::string, Value>>(args[1]);
        for (const auto& pair : dict2) {
            dict1[pair.first] = pair.second;
        }
        return Value();
    }

    Value Builtins::timeNow(const std::vector<Value>& args) {
        auto now = std::chrono::system_clock::now();
        auto duration = now.time_since_epoch();
        return Value(static_cast<double>(std::chrono::duration_cast<std::chrono::milliseconds>(duration).count()) / 1000.0);
    }

    Value Builtins::timeSleep(const std::vector<Value>& args) {
        if (args.size() != 1 || !std::holds_alternative<double>(args[0])) {
            throw std::runtime_error("time_sleep expects one number");
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(
            static_cast<long>(std::get<double>(args[0]) * 1000)));
        return Value();
    }

    Value Builtins::systemExit(const std::vector<Value>& args) {
        int exitCode = 0;
        if (args.size() > 0 && std::holds_alternative<int64_t>(args[0])) {
            exitCode = static_cast<int>(std::get<int64_t>(args[0]));
        }
        std::exit(exitCode);
        return Value();
    }

    Value Builtins::systemEnv(const std::vector<Value>& args) {
        if (args.size() != 1 || !std::holds_alternative<std::string>(args[0])) {
            throw std::runtime_error("system_env expects one string");
        }
        const char* value = std::getenv(std::get<std::string>(args[0]).c_str());
        return value ? Value(std::string(value)) : Value();
    }

    Value Builtins::systemCwd(const std::vector<Value>& args) {
        return Value(std::filesystem::current_path().string());
    }

    Value Builtins::systemExec(const std::vector<Value>& args) {
        if (args.size() != 1 || !std::holds_alternative<std::string>(args[0])) {
            throw std::runtime_error("system_exec expects one string");
        }
        std::string command = std::get<std::string>(args[0]);
        std::array<char, 128> buffer;
        std::string result;
        std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(command.c_str(), "r"), pclose);
        if (!pipe) {
            throw std::runtime_error("popen() failed");
        }
        while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
            result += buffer.data();
        }
        return Value(result);
    }

    Value Builtins::toInt(const std::vector<Value>& args) {
        if (args.size() != 1) {
            throw std::runtime_error("to_int expects one argument");
        }
        if (std::holds_alternative<int64_t>(args[0])) {
            return args[0];
        }
        if (std::holds_alternative<double>(args[0])) {
            return Value(static_cast<int64_t>(std::get<double>(args[0])));
        }
        if (std::holds_alternative<std::string>(args[0])) {
            try {
                return Value(std::stoll(std::get<std::string>(args[0])));
            } catch (...) {
                throw std::runtime_error("Cannot convert string to integer");
            }
        }
        throw std::runtime_error("Cannot convert to integer");
    }

    Value Builtins::toFloat(const std::vector<Value>& args) {
        if (args.size() != 1) {
            throw std::runtime_error("to_float expects one argument");
        }
        if (std::holds_alternative<double>(args[0])) {
            return args[0];
        }
        if (std::holds_alternative<int64_t>(args[0])) {
            return Value(static_cast<double>(std::get<int64_t>(args[0])));
        }
        if (std::holds_alternative<std::string>(args[0])) {
            try {
                return Value(std::stod(std::get<std::string>(args[0])));
            } catch (...) {
                throw std::runtime_error("Cannot convert string to float");
            }
        }
        throw std::runtime_error("Cannot convert to float");
    }

    Value Builtins::toString(const std::vector<Value>& args) {
        if (args.size() != 1) {
            throw std::runtime_error("to_string expects one argument");
        }
        std::stringstream ss;
        std::visit([&ss](const auto& v) { ss << v; }, args[0]);
        return Value(ss.str());
    }

    Value Builtins::toBool(const std::vector<Value>& args) {
        if (args.size() != 1) {
            throw std::runtime_error("to_bool expects one argument");
        }
        if (std::holds_alternative<bool>(args[0])) {
            return args[0];
        }
        if (std::holds_alternative<int64_t>(args[0])) {
            return Value(std::get<int64_t>(args[0]) != 0);
        }
        if (std::holds_alternative<double>(args[0])) {
            return Value(std::get<double>(args[0]) != 0.0);
        }
        if (std::holds_alternative<std::string>(args[0])) {
            return Value(!std::get<std::string>(args[0]).empty());
        }
        return Value(false);
    }

    Value Builtins::toArray(const std::vector<Value>& args) {
        if (args.size() != 1 || !std::holds_alternative<std::vector<Value>>(args[0])) {
            throw std::runtime_error("to_array expects one array");
        }
        return args[0];
    }

    Value Builtins::toDict(const std::vector<Value>& args) {
        if (args.size() != 1 || !std::holds_alternative<std::unordered_map<std::string, Value>>(args[0])) {
            throw std::runtime_error("to_dict expects one dictionary");
        }
        return args[0];
    }

} // namespace interpreter 