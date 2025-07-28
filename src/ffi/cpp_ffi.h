#pragma once

#include "../ast/types.h"
#include "../ast/ast.h"
#include "../error/error_handler.h"
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <typeinfo>

namespace ffi {

/**
 * @brief C++ FFI interface for calling C++ code from Tocin
 */
class CppFFI {
public:
    CppFFI();
    ~CppFFI();

    // Library management
    bool loadLibrary(const std::string& libraryPath);
    bool unloadLibrary(const std::string& libraryPath);
    bool isLibraryLoaded(const std::string& libraryPath);

    // Symbol resolution
    void* getSymbol(const std::string& libraryPath, const std::string& symbolName);
    bool hasSymbol(const std::string& libraryPath, const std::string& symbolName);

    // C++ value representation
    struct CppValue {
        enum Type { 
            VOID_TYPE, BOOL, CHAR, SHORT, INT, LONG, LONG_LONG,
            UNSIGNED_CHAR, UNSIGNED_SHORT, UNSIGNED_INT, UNSIGNED_LONG, UNSIGNED_LONG_LONG,
            FLOAT, DOUBLE, LONG_DOUBLE,
            POINTER, REFERENCE, ARRAY, STRUCT, CLASS, ENUM,
            STRING, WSTRING
        };
        
        Type type;
        size_t size;
        void* data;
        std::string typeName;
        bool isConst;
        bool isVolatile;
        
        CppValue() : type(VOID_TYPE), size(0), data(nullptr), isConst(false), isVolatile(false) {}
        CppValue(Type t, size_t s, void* d, const std::string& name = "") 
            : type(t), size(s), data(d), typeName(name), isConst(false), isVolatile(false) {}
        
        ~CppValue() {
            if (data && type != POINTER && type != REFERENCE) {
                free(data);
            }
        }
        
        // Copy constructor
        CppValue(const CppValue& other) 
            : type(other.type), size(other.size), typeName(other.typeName), 
              isConst(other.isConst), isVolatile(other.isVolatile) {
            if (other.data && other.size > 0) {
                data = malloc(other.size);
                memcpy(data, other.data, other.size);
            } else {
                data = other.data;
            }
        }
        
        // Move constructor
        CppValue(CppValue&& other) noexcept
            : type(other.type), size(other.size), data(other.data), 
              typeName(std::move(other.typeName)), isConst(other.isConst), isVolatile(other.isVolatile) {
            other.data = nullptr;
            other.size = 0;
        }
    };

    // Function calling
    struct FunctionSignature {
        std::string name;
        CppValue::Type returnType;
        std::vector<CppValue::Type> paramTypes;
        std::string mangledName;
        void* functionPtr;
        
        FunctionSignature() : functionPtr(nullptr) {}
    };

    bool registerFunction(const std::string& libraryPath, const FunctionSignature& signature);
    CppValue callFunction(const std::string& functionName, const std::vector<CppValue>& args);
    CppValue callFunctionPtr(void* functionPtr, const FunctionSignature& signature, const std::vector<CppValue>& args);

    // Class operations
    struct ClassInfo {
        std::string name;
        size_t size;
        std::unordered_map<std::string, size_t> memberOffsets;
        std::unordered_map<std::string, CppValue::Type> memberTypes;
        std::unordered_map<std::string, FunctionSignature> methods;
        std::vector<std::string> baseClasses;
        void* vtablePtr;
        
        ClassInfo() : size(0), vtablePtr(nullptr) {}
    };

    bool registerClass(const std::string& libraryPath, const ClassInfo& classInfo);
    CppValue createInstance(const std::string& className, const std::vector<CppValue>& constructorArgs);
    bool destroyInstance(const std::string& className, CppValue& instance);
    
    CppValue getMember(const CppValue& instance, const std::string& memberName);
    bool setMember(CppValue& instance, const std::string& memberName, const CppValue& value);
    
    CppValue callMethod(CppValue& instance, const std::string& methodName, const std::vector<CppValue>& args);
    CppValue callStaticMethod(const std::string& className, const std::string& methodName, const std::vector<CppValue>& args);

    // Template support
    struct TemplateInfo {
        std::string name;
        std::vector<std::string> typeParameters;
        std::unordered_map<std::string, ClassInfo> instantiations;
    };

    bool registerTemplate(const std::string& libraryPath, const TemplateInfo& templateInfo);
    std::string instantiateTemplate(const std::string& templateName, const std::vector<std::string>& typeArgs);

    // STL container support
    CppValue createVector(CppValue::Type elementType, const std::vector<CppValue>& elements);
    CppValue createMap(CppValue::Type keyType, CppValue::Type valueType, 
                      const std::vector<std::pair<CppValue, CppValue>>& pairs);
    CppValue createSet(CppValue::Type elementType, const std::vector<CppValue>& elements);
    
    size_t getContainerSize(const CppValue& container);
    CppValue getContainerElement(const CppValue& container, size_t index);
    bool setContainerElement(CppValue& container, size_t index, const CppValue& value);
    bool insertIntoContainer(CppValue& container, const CppValue& value);

    // Memory management
    CppValue allocateMemory(size_t size, CppValue::Type type);
    bool deallocateMemory(CppValue& value);
    CppValue createReference(CppValue& value);
    CppValue dereference(const CppValue& pointer);

    // Type conversion
    CppValue toCppValue(ast::ValuePtr value);
    ast::ValuePtr fromCppValue(const CppValue& value);
    
    // Type information
    std::string getTypeName(const CppValue& value);
    size_t getTypeSize(CppValue::Type type);
    bool isPointerType(const CppValue& value);
    bool isReferenceType(const CppValue& value);
    bool isClassType(const CppValue& value);

    // Exception handling
    struct CppException {
        std::string type;
        std::string message;
        void* exceptionPtr;
        
        CppException() : exceptionPtr(nullptr) {}
        CppException(const std::string& t, const std::string& m, void* ptr = nullptr)
            : type(t), message(m), exceptionPtr(ptr) {}
    };

    bool hasException() const { return hasException_; }
    CppException getLastException() const { return lastException_; }
    void clearException() { hasException_ = false; lastException_ = CppException(); }

    // Error handling
    bool hasError() const { return hasError_; }
    std::string getLastError() const { return lastError_; }

private:
    std::unordered_map<std::string, void*> loadedLibraries_;
    std::unordered_map<std::string, FunctionSignature> registeredFunctions_;
    std::unordered_map<std::string, ClassInfo> registeredClasses_;
    std::unordered_map<std::string, TemplateInfo> registeredTemplates_;
    
    bool hasError_;
    std::string lastError_;
    bool hasException_;
    CppException lastException_;

    void setError(const std::string& error);
    void clearError();
    void setException(const CppException& exception);
    
    // Platform-specific implementations
    void* loadLibraryImpl(const std::string& path);
    bool unloadLibraryImpl(void* handle);
    void* getSymbolImpl(void* handle, const std::string& name);
    
    // Type marshalling
    void* marshalValue(const CppValue& value);
    CppValue unmarshalValue(void* data, CppValue::Type type, size_t size);
    
    // Function call implementation
    CppValue callFunctionImpl(void* functionPtr, const FunctionSignature& signature, 
                             const std::vector<CppValue>& args);
};

/**
 * @brief C++ standard library integration
 */
class StdLibIntegration {
public:
    static bool initialize();
    static bool isAvailable();

    // String operations
    static CppFFI::CppValue createString(const std::string& value);
    static std::string getString(const CppFFI::CppValue& cppString);
    static CppFFI::CppValue stringConcat(const CppFFI::CppValue& a, const CppFFI::CppValue& b);
    static size_t stringLength(const CppFFI::CppValue& cppString);

    // Vector operations
    static CppFFI::CppValue createIntVector(const std::vector<int>& values);
    static CppFFI::CppValue createDoubleVector(const std::vector<double>& values);
    static std::vector<int> getIntVector(const CppFFI::CppValue& cppVector);
    static std::vector<double> getDoubleVector(const CppFFI::CppValue& cppVector);

    // Map operations
    static CppFFI::CppValue createStringIntMap(const std::unordered_map<std::string, int>& values);
    static std::unordered_map<std::string, int> getStringIntMap(const CppFFI::CppValue& cppMap);

    // Algorithm operations
    static CppFFI::CppValue sort(const CppFFI::CppValue& container);
    static CppFFI::CppValue find(const CppFFI::CppValue& container, const CppFFI::CppValue& value);
    static CppFFI::CppValue transform(const CppFFI::CppValue& container, void* transformFunc);

private:
    static bool initialized_;
    static CppFFI* cppFFI_;
};

/**
 * @brief Boost library integration
 */
class BoostIntegration {
public:
    static bool initialize();
    static bool isAvailable();

    // Boost.Regex
    static bool regexMatch(const std::string& text, const std::string& pattern);
    static std::vector<std::string> regexSearch(const std::string& text, const std::string& pattern);
    static std::string regexReplace(const std::string& text, const std::string& pattern, const std::string& replacement);

    // Boost.Filesystem
    static bool pathExists(const std::string& path);
    static bool isDirectory(const std::string& path);
    static bool isFile(const std::string& path);
    static std::vector<std::string> listDirectory(const std::string& path);
    static bool createDirectory(const std::string& path);
    static bool removeFile(const std::string& path);

    // Boost.Thread
    static void* createThread(void* (*func)(void*), void* arg);
    static bool joinThread(void* thread);
    static void* createMutex();
    static bool lockMutex(void* mutex);
    static bool unlockMutex(void* mutex);

    // Boost.Serialization
    static std::string serializeToXML(const CppFFI::CppValue& value);
    static CppFFI::CppValue deserializeFromXML(const std::string& xml, const std::string& typeName);

private:
    static bool initialized_;
    static CppFFI* cppFFI_;
};

} // namespace ffi