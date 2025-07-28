#pragma once

#include "../ast/types.h"
#include "../ast/ast.h"
#include "../error/error_handler.h"
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>

namespace ffi {

/**
 * @brief Python FFI interface for calling Python code from Tocin
 */
class PythonFFI {
public:
    PythonFFI();
    ~PythonFFI();

    // Initialize Python interpreter
    bool initialize();
    void finalize();

    // Module management
    bool importModule(const std::string& moduleName);
    bool importModuleAs(const std::string& moduleName, const std::string& alias);
    bool importFromModule(const std::string& moduleName, const std::string& functionName);

    // Function calling
    struct PythonValue {
        enum Type { NONE, BOOL, INT, FLOAT, STRING, LIST, DICT, OBJECT };
        Type type;
        union {
            bool boolValue;
            long intValue;
            double floatValue;
        };
        std::string stringValue;
        std::vector<PythonValue> listValue;
        std::unordered_map<std::string, PythonValue> dictValue;
        void* objectPtr;

        PythonValue() : type(NONE), objectPtr(nullptr) {}
        PythonValue(bool value) : type(BOOL), boolValue(value) {}
        PythonValue(long value) : type(INT), intValue(value) {}
        PythonValue(double value) : type(FLOAT), floatValue(value) {}
        PythonValue(const std::string& value) : type(STRING), stringValue(value) {}
        PythonValue(const std::vector<PythonValue>& value) : type(LIST), listValue(value) {}
        PythonValue(const std::unordered_map<std::string, PythonValue>& value) : type(DICT), dictValue(value) {}
    };

    PythonValue callFunction(const std::string& functionName, const std::vector<PythonValue>& args);
    PythonValue callMethod(const PythonValue& object, const std::string& methodName, const std::vector<PythonValue>& args);

    // Type conversion
    PythonValue toPythonValue(ast::ValuePtr value);
    ast::ValuePtr fromPythonValue(const PythonValue& value);

    // Error handling
    bool hasError() const { return hasError_; }
    std::string getLastError() const { return lastError_; }

    // Code execution
    PythonValue executeCode(const std::string& code);
    bool executeFile(const std::string& filename);

    // Object attribute access
    PythonValue getAttribute(const PythonValue& object, const std::string& attrName);
    bool setAttribute(const PythonValue& object, const std::string& attrName, const PythonValue& value);

    // List operations
    PythonValue createList(const std::vector<PythonValue>& items);
    bool appendToList(PythonValue& list, const PythonValue& item);
    PythonValue getListItem(const PythonValue& list, size_t index);
    bool setListItem(PythonValue& list, size_t index, const PythonValue& value);

    // Dictionary operations
    PythonValue createDict(const std::unordered_map<std::string, PythonValue>& items);
    PythonValue getDictItem(const PythonValue& dict, const std::string& key);
    bool setDictItem(PythonValue& dict, const std::string& key, const PythonValue& value);

private:
    bool initialized_;
    bool hasError_;
    std::string lastError_;
    void* pythonState_;  // PyObject* in actual implementation
    std::unordered_map<std::string, void*> modules_;  // Module cache

    void setError(const std::string& error);
    void clearError();
    void* toPyObject(const PythonValue& value);
    PythonValue fromPyObject(void* pyObj);
};

/**
 * @brief Python decorator for Tocin functions
 */
class PythonDecorator {
public:
    PythonDecorator(const std::string& decoratorCode) : decoratorCode_(decoratorCode) {}

    // Apply decorator to a Tocin function
    bool decorateFunction(const std::string& functionName, ast::FunctionDeclPtr function);

    // Get decorated function result
    PythonFFI::PythonValue callDecoratedFunction(const std::string& functionName, 
                                                 const std::vector<PythonFFI::PythonValue>& args);

private:
    std::string decoratorCode_;
    std::unordered_map<std::string, void*> decoratedFunctions_;
};

/**
 * @brief Python class wrapper for Tocin classes
 */
class PythonClassWrapper {
public:
    PythonClassWrapper(const std::string& className, ast::ClassDeclPtr classDecl);

    // Create Python class from Tocin class
    bool createPythonClass();

    // Create instance of Python class
    PythonFFI::PythonValue createInstance(const std::vector<PythonFFI::PythonValue>& args);

    // Call method on Python instance
    PythonFFI::PythonValue callMethod(const PythonFFI::PythonValue& instance, 
                                     const std::string& methodName,
                                     const std::vector<PythonFFI::PythonValue>& args);

private:
    std::string className_;
    ast::ClassDeclPtr classDecl_;
    void* pythonClass_;  // PyObject* in actual implementation
};

/**
 * @brief Python numpy integration
 */
class NumpyIntegration {
public:
    static bool initialize();
    static bool isAvailable();

    // Array operations
    static PythonFFI::PythonValue createArray(const std::vector<double>& data, 
                                             const std::vector<size_t>& shape);
    static std::vector<double> arrayToVector(const PythonFFI::PythonValue& array);
    
    // Mathematical operations
    static PythonFFI::PythonValue add(const PythonFFI::PythonValue& a, const PythonFFI::PythonValue& b);
    static PythonFFI::PythonValue multiply(const PythonFFI::PythonValue& a, const PythonFFI::PythonValue& b);
    static PythonFFI::PythonValue matmul(const PythonFFI::PythonValue& a, const PythonFFI::PythonValue& b);
    
    // Statistical operations
    static PythonFFI::PythonValue mean(const PythonFFI::PythonValue& array);
    static PythonFFI::PythonValue std(const PythonFFI::PythonValue& array);
    static PythonFFI::PythonValue sum(const PythonFFI::PythonValue& array);

private:
    static bool initialized_;
    static void* numpyModule_;
};

/**
 * @brief Python pandas integration
 */
class PandasIntegration {
public:
    static bool initialize();
    static bool isAvailable();

    // DataFrame operations
    static PythonFFI::PythonValue createDataFrame(const std::unordered_map<std::string, std::vector<PythonFFI::PythonValue>>& data);
    static PythonFFI::PythonValue readCsv(const std::string& filename);
    static bool toCsv(const PythonFFI::PythonValue& dataframe, const std::string& filename);
    
    // Data manipulation
    static PythonFFI::PythonValue select(const PythonFFI::PythonValue& dataframe, const std::vector<std::string>& columns);
    static PythonFFI::PythonValue filter(const PythonFFI::PythonValue& dataframe, const std::string& condition);
    static PythonFFI::PythonValue groupBy(const PythonFFI::PythonValue& dataframe, const std::vector<std::string>& columns);

private:
    static bool initialized_;
    static void* pandasModule_;
};

} // namespace ffi