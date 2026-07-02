#ifndef TOCIN_BUILTIN_NAMES_H
#define TOCIN_BUILTIN_NAMES_H

#include <set>
#include <string>
#include <unordered_map>

namespace type_checker
{
    // Canonical registry of compiler builtins: name -> allowed argument counts.
    // An EMPTY set means "any arity" (used for names whose dispatch is variadic
    // or too irregular to pin down).
    //
    // This list mirrors the `funcName == "..."` dispatch in
    // src/codegen/ir_generator.cpp, which is the source of truth. When a builtin
    // is added there, add it here too - otherwise strict type checking will
    // reject programs that use it ("undeclared identifier").
    inline const std::unordered_map<std::string, std::set<int>> &builtinArities()
    {
        static const std::unordered_map<std::string, std::set<int>> table = {
            // I/O and process
            {"print", {}}, {"println", {}}, {"printf", {}}, {"input", {}},
            {"readLine", {0}}, {"readFile", {1}}, {"writeFile", {2}}, {"appendFile", {2}},
            {"envGet", {1}}, {"sysExit", {1}}, {"sleepMs", {1}},
            // time
            {"timeMs", {0}}, {"timeSec", {0}}, {"monoNanos", {0}},
            // conversions
            {"intToStr", {1}}, {"strToInt", {1}}, {"floatToStr", {1}}, {"strToFloat", {1}},
            {"intToFloat", {1}}, {"floatToInt", {1}}, {"charToStr", {1}},
            {"str", {}}, {"int", {}}, {"float", {}}, {"bool", {}},
            // strings
            {"strLen", {1}}, {"strEq", {2}}, {"strCmp", {2}}, {"strContains", {2}},
            {"strIndexOf", {2}}, {"substring", {3}}, {"charAt", {2}}, {"indexOfChar", {2}},
            {"startsWith", {2}}, {"endsWith", {2}}, {"toLower", {1}}, {"toUpper", {1}},
            {"toLowerChar", {1}}, {"toUpperChar", {1}},
            {"isDigit", {1}}, {"isAlpha", {1}}, {"isAlnum", {1}}, {"isSpace", {1}},
            {"isLower", {1}}, {"isUpper", {1}},
            {"len", {1}},
            // hashing / random
            {"hashInt", {1}}, {"hashStr", {1}}, {"hashBytes", {2}},
            {"randSeed", {1}}, {"randInt", {0}}, {"randRange", {2}},
            // math (unary libm set + intrinsic-lowered subset)
            {"sqrt", {1}}, {"sin", {1}}, {"cos", {1}}, {"tan", {1}},
            {"asin", {1}}, {"acos", {1}}, {"atan", {1}},
            {"exp", {1}}, {"log", {1}}, {"log2", {1}}, {"log10", {1}},
            {"floor", {1}}, {"ceil", {1}}, {"round", {1}}, {"fabs", {1}},
            {"pow", {2}}, {"abs", {1}}, {"absInt", {1}}, {"min", {}}, {"max", {}},
            {"range", {}},
            // raw memory (systems/kernel fast path)
            {"alloc", {1}}, {"free", {1}}, {"memcpy", {3}}, {"memset", {3}},
            {"bufToStr", {2}}, {"strFromAddr", {1}},
            {"ptrAdd", {2}}, {"loadByte", {2}}, {"storeByte", {3}},
            {"loadInt", {2}}, {"storeInt", {3}},
            // volatile MMIO access + barriers (kernel/driver work)
            {"volatileLoad8", {2}}, {"volatileLoad16", {2}},
            {"volatileLoad32", {2}}, {"volatileLoad64", {2}},
            {"volatileStore8", {3}}, {"volatileStore16", {3}},
            {"volatileStore32", {3}}, {"volatileStore64", {3}},
            {"fence", {0}},
            // inline assembly: asm(template) or asm(template, constraints, ...)
            {"asm", {}},
            // growable vector runtime
            {"vecNew", {0}}, {"vecPush", {2}}, {"vecGet", {2}}, {"vecSet", {3}},
            {"vecLen", {1}}, {"vecPop", {1}}, {"vecFree", {1}}, {"vecToArray", {1}},
            // hashmap runtime
            {"mapNew", {0}}, {"mapPut", {3}}, {"mapGet", {2}}, {"mapHas", {2}},
            {"mapPutStr", {3}}, {"mapGetStr", {2}}, {"mapHasStr", {2}},
            {"mapLen", {1}}, {"mapFree", {1}},
            // networking
            {"tcpListen", {1}}, {"tcpAccept", {1}}, {"tcpConnect", {2}},
            {"tcpSend", {2}}, {"tcpRecv", {1}}, {"tcpClose", {1}},
            // Option/Result constructors and concurrency
            {"Some", {1}}, {"Ok", {1}}, {"Err", {1}}, {"__chan_new", {}},
        };
        return table;
    }

    inline bool isBuiltinName(const std::string &name)
    {
        return builtinArities().count(name) > 0;
    }
} // namespace type_checker

#endif // TOCIN_BUILTIN_NAMES_H
