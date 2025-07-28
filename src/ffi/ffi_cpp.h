#pragma once

#include "ffi_interface.h"
#include "ffi_value.h"
#include "cpp_ffi.h"
#include "../ast/types.h"
#include "../error/error_handler.h"
#include <memory>
#include <string>
#include <vector>

namespace ffi {

/**
 * @brief C++ FFI implementation that conforms to FFIInterface
 */
class CppFFIImpl : public FFIInterface {
public:
    CppFFIImpl();
    ~CppFFIImpl() override;

    // FFIInterface implementation
    bool initialize() override;
    void finalize() override;
    bool isInitialized() const override { return initialized_; }

    std::string getLanguageName() const override { return "C++"; }
    std::string getVersion() const override;

    FFIValue callFunction(const std::string& functionName, const std::vector<FFIValue>& args) override;
    bool hasFunction(const std::string& functionName) const override;

    bool loadModule(const std::string& moduleName) override;
    bool unloadModule(const std::string& moduleName) override;
    bool isModuleLoaded(const std::string& moduleName) const override;

    FFIValue toFFIValue(ast::ValuePtr value) override;
    ast::ValuePtr fromFFIValue(const FFIValue& value) override;

    bool hasError() const override;
    std::string getLastError() const override;
    void clearError() override;

    std::vector<std::string> getSupportedFeatures() const override;
    bool supportsFeature(const std::string& feature) const override;

    // C++-specific methods
    bool loadLibrary(const std::string& libraryPath);
    bool unloadLibrary(const std::string& libraryPath);
    bool isLibraryLoaded(const std::string& libraryPath) const;

    void* getSymbol(const std::string& libraryPath, const std::string& symbolName);
    bool hasSymbol(const std::string& libraryPath, const std::string& symbolName) const;

    // Function registration and calling
    bool registerFunction(const std::string& libraryPath, const CppFFI::FunctionSignature& signature);
    FFIValue callFunctionPtr(void* functionPtr, const CppFFI::FunctionSignature& signature, 
                            const std::vector<FFIValue>& args);

    // Class operations
    bool registerClass(const std::string& libraryPath, const CppFFI::ClassInfo& classInfo);
    FFIValue createInstance(const std::string& className, const std::vector<FFIValue>& constructorArgs);
    bool destroyInstance(const std::string& className, FFIValue& instance);
    
    FFIValue getMember(const FFIValue& instance, const std::string& memberName);
    bool setMember(FFIValue& instance, const std::string& memberName, const FFIValue& value);
    
    FFIValue callMethod(FFIValue& instance, const std::string& methodName, const std::vector<FFIValue>& args);
    FFIValue callStaticMethod(const std::string& className, const std::string& methodName, 
                             const std::vector<FFIValue>& args);

    // Template support
    bool registerTemplate(const std::string& libraryPath, const CppFFI::TemplateInfo& templateInfo);
    std::string instantiateTemplate(const std::string& templateName, const std::vector<std::string>& typeArgs);

    // STL container support
    FFIValue createVector(const std::string& elementType, const std::vector<FFIValue>& elements);
    FFIValue createMap(const std::string& keyType, const std::string& valueType, 
                      const std::vector<std::pair<FFIValue, FFIValue>>& pairs);
    FFIValue createSet(const std::string& elementType, const std::vector<FFIValue>& elements);

    // Memory management
    FFIValue allocateMemory(size_t size, const std::string& typeName);
    bool deallocateMemory(FFIValue& value);
    FFIValue createReference(FFIValue& value);
    FFIValue dereference(const FFIValue& pointer);

    // Exception handling
    bool hasException() const;
    CppFFI::CppException getLastException() const;
    void clearException();

private:
    bool initialized_;
    std::unique_ptr<CppFFI> cppFFI_;
    
    // Conversion helpers
    CppFFI::CppValue ffiValueToCpp(const FFIValue& value);
    FFIValue cppValueToFFI(const CppFFI::CppValue& value);
};

/**
 * @brief C++ library wrapper for easier library management
 */
class CppLibrary {
public:
    CppLibrary(const std::string& path, CppFFIImpl* ffi);
    ~CppLibrary();

    bool load();
    bool unload();
    bool isLoaded() const { return loaded_; }

    void* getSymbol(const std::string& symbolName);
    bool hasSymbol(const std::string& symbolName) const;

    bool registerFunction(const CppFFI::FunctionSignature& signature);
    bool registerClass(const CppFFI::ClassInfo& classInfo);
    bool registerTemplate(const CppFFI::TemplateInfo& templateInfo);

    const std::string& getPath() const { return path_; }

private:
    std::string path_;
    CppFFIImpl* ffi_;
    bool loaded_;
    void* handle_;
};

/**
 * @brief C++ class wrapper for easier class instantiation and method calling
 */
class CppClass {
public:
    CppClass(const std::string& className, const std::string& libraryPath, CppFFIImpl* ffi);
    ~CppClass();

    FFIValue createInstance(const std::vector<FFIValue>& constructorArgs = {});
    bool destroyInstance(FFIValue& instance);
    
    FFIValue callStaticMethod(const std::string& methodName, const std::vector<FFIValue>& args);
    
    bool hasMethod(const std::string& methodName) const;
    bool hasStaticMethod(const std::string& methodName) const;
    bool hasMember(const std::string& memberName) const;

    const std::string& getClassName() const { return className_; }
    const std::string& getLibraryPath() const { return libraryPath_; }

private:
    std::string className_;
    std::string libraryPath_;
    CppFFIImpl* ffi_;
    CppFFI::ClassInfo classInfo_;
};

/**
 * @brief C++ instance wrapper for easier method calling and member access
 */
class CppInstance {
public:
    CppInstance(const FFIValue& instance, const std::string& className, CppFFIImpl* ffi);
    ~CppInstance();

    FFIValue callMethod(const std::string& methodName, const std::vector<FFIValue>& args);
    
    FFIValue getMember(const std::string& memberName);
    bool setMember(const std::string& memberName, const FFIValue& value);
    
    bool hasMethod(const std::string& methodName) const;
    bool hasMember(const std::string& memberName) const;

    const FFIValue& getInstance() const { return instance_; }
    const std::string& getClassName() const { return className_; }

private:
    FFIValue instance_;
    std::string className_;
    CppFFIImpl* ffi_;
};

/**
 * @brief C++ template wrapper for easier template instantiation
 */
class CppTemplate {
public:
    CppTemplate(const std::string& templateName, const std::string& libraryPath, CppFFIImpl* ffi);
    ~CppTemplate();

    std::string instantiate(const std::vector<std::string>& typeArgs);
    bool hasInstantiation(const std::vector<std::string>& typeArgs) const;
    
    std::vector<std::string> getTypeParameters() const;
    const std::string& getTemplateName() const { return templateName_; }

private:
    std::string templateName_;
    std::string libraryPath_;
    CppFFIImpl* ffi_;
    CppFFI::TemplateInfo templateInfo_;
};

/**
 * @brief C++ STL integration utilities
 */
class CppStdLib {
public:
    static bool initialize(CppFFIImpl* ffi);
    static bool isAvailable();

    // String operations
    static FFIValue createString(const std::string& value);
    static std::string getString(const FFIValue& cppString);
    static FFIValue stringConcat(const FFIValue& a, const FFIValue& b);
    static size_t stringLength(const FFIValue& cppString);

    // Vector operations
    static FFIValue createIntVector(const std::vector<int>& values);
    static FFIValue createDoubleVector(const std::vector<double>& values);
    static FFIValue createStringVector(const std::vector<std::string>& values);
    static std::vector<int> getIntVector(const FFIValue& cppVector);
    static std::vector<double> getDoubleVector(const FFIValue& cppVector);
    static std::vector<std::string> getStringVector(const FFIValue& cppVector);

    // Map operations
    static FFIValue createStringIntMap(const std::unordered_map<std::string, int>& values);
    static FFIValue createStringDoubleMap(const std::unordered_map<std::string, double>& values);
    static std::unordered_map<std::string, int> getStringIntMap(const FFIValue& cppMap);
    static std::unordered_map<std::string, double> getStringDoubleMap(const FFIValue& cppMap);

    // Set operations
    static FFIValue createIntSet(const std::vector<int>& values);
    static FFIValue createStringSet(const std::vector<std::string>& values);
    static std::vector<int> getIntSet(const FFIValue& cppSet);
    static std::vector<std::string> getStringSet(const FFIValue& cppSet);

    // Algorithm operations
    static FFIValue sort(const FFIValue& container);
    static FFIValue find(const FFIValue& container, const FFIValue& value);
    static FFIValue transform(const FFIValue& container, void* transformFunc);

private:
    static bool initialized_;
    static CppFFIImpl* ffi_;
};

/**
 * @brief C++ integration utilities
 */
class CppUtils {
public:
    // Name mangling utilities
    static std::string mangleFunctionName(const std::string& functionName, 
                                         const std::vector<std::string>& paramTypes);
    static std::string demangleFunctionName(const std::string& mangledName);
    
    // Type utilities
    static std::string tocinTypeToCpp(const std::string& tocinType);
    static std::string cppTypeToTocin(const std::string& cppType);
    static size_t getTypeSize(const std::string& cppType);
    
    // Code generation utilities
    static std::string generateCppWrapper(const std::string& functionName, 
                                         const std::vector<std::string>& paramTypes,
                                         const std::string& returnType);
    static std::string generateClassWrapper(const std::string& className,
                                           const std::vector<std::string>& methods);

    // Error handling utilities
    static bool isCppException(const FFIValue& value);
    static std::string extractCppException(const FFIValue& value);
    static FFIValue createCppException(const std::string& exceptionType, const std::string& message);

    // Memory utilities
    static size_t getObjectSize(const FFIValue& object);
    static bool isValidPointer(const FFIValue& pointer);
    static FFIValue cloneObject(const FFIValue& object);

    // Performance utilities
    static void enableCppProfiling(bool enable);
    static std::string getCppProfilingResults();

private:
    CppUtils() = delete;
};

} // namespace ffi