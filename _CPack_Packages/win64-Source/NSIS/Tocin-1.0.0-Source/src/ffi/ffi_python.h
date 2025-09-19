#pragma once

#include "ffi_interface.h"
#include "ffi_value.h"
#include "../ast/types.h"
#include "../error/error_handler.h"
#include <memory>
#include <string>
#include <vector>

namespace ffi {

/**
 * @brief Python FFI implementation that conforms to FFIInterface
 */
class PythonFFIImpl : public FFIInterface {
public:
    PythonFFIImpl();
    ~PythonFFIImpl() override;

    // FFIInterface implementation
    bool initialize() override;
    void finalize() override;
    bool isInitialized() const override { return initialized_; }

    std::string getLanguageName() const override { return "Python"; }
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

    FFIValue eval(const std::string& code) override;
    FFIValue getVariable(const std::string& name) override;
    void setVariable(const std::string& name, const FFIValue& value) override;
    bool isAvailable() const override;

    // Python-specific methods
    FFIValue executeCode(const std::string& code);
    bool executeFile(const std::string& filename);
    
    FFIValue callMethod(const FFIValue& object, const std::string& methodName, const std::vector<FFIValue>& args);
    
    FFIValue getAttribute(const FFIValue& object, const std::string& attrName);
    bool setAttribute(FFIValue& object, const std::string& attrName, const FFIValue& value);

    // Python object creation
    FFIValue createList(const std::vector<FFIValue>& items);
    FFIValue createDict(const std::unordered_map<std::string, FFIValue>& items);
    FFIValue createTuple(const std::vector<FFIValue>& items);

    // Python-specific type checking
    bool isPythonObject(const FFIValue& value) const;
    std::string getPythonTypeName(const FFIValue& value) const;

private:
    bool initialized_;
    std::unique_ptr<PythonFFI> pythonFFI_;
    
    // Conversion helpers
    FFIValue ffiValueToPython(const FFIValue& value);
    FFIValue pythonValueToFFI(const FFIValue& value);
};

/**
 * @brief Python module wrapper for easier module management
 */
class PythonModule {
public:
    PythonModule(const std::string& name, PythonFFIImpl* ffi);
    ~PythonModule();

    bool load();
    bool unload();
    bool isLoaded() const { return loaded_; }

    FFIValue callFunction(const std::string& functionName, const std::vector<FFIValue>& args);
    bool hasFunction(const std::string& functionName) const;

    FFIValue getAttribute(const std::string& attrName);
    bool setAttribute(const std::string& attrName, const FFIValue& value);

    std::vector<std::string> getFunctionNames() const;
    std::vector<std::string> getAttributeNames() const;

    const std::string& getName() const { return name_; }

private:
    std::string name_;
    PythonFFIImpl* ffi_;
    bool loaded_;
    FFIValue moduleObject_;
};

/**
 * @brief Python class wrapper for easier class instantiation and method calling
 */
class PythonClass {
public:
    PythonClass(const std::string& className, const std::string& moduleName, PythonFFIImpl* ffi);
    ~PythonClass();

    FFIValue createInstance(const std::vector<FFIValue>& args = {});
    FFIValue callStaticMethod(const std::string& methodName, const std::vector<FFIValue>& args);
    
    bool hasMethod(const std::string& methodName) const;
    bool hasStaticMethod(const std::string& methodName) const;

    const std::string& getClassName() const { return className_; }
    const std::string& getModuleName() const { return moduleName_; }

private:
    std::string className_;
    std::string moduleName_;
    PythonFFIImpl* ffi_;
    FFIValue classObject_;
};

/**
 * @brief Python instance wrapper for easier method calling and attribute access
 */
class PythonInstance {
public:
    PythonInstance(const FFIValue& instance, PythonFFIImpl* ffi);
    ~PythonInstance();

    FFIValue callMethod(const std::string& methodName, const std::vector<FFIValue>& args);
    
    FFIValue getAttribute(const std::string& attrName);
    bool setAttribute(const std::string& attrName, const FFIValue& value);
    
    bool hasMethod(const std::string& methodName) const;
    bool hasAttribute(const std::string& attrName) const;

    const FFIValue& getInstance() const { return instance_; }
    std::string getTypeName() const;

private:
    FFIValue instance_;
    PythonFFIImpl* ffi_;
};

/**
 * @brief Python decorator support for Tocin functions
 */
class PythonDecorators {
public:
    static bool registerDecorator(const std::string& name, const std::string& pythonCode);
    static bool applyDecorator(const std::string& decoratorName, const std::string& functionName, 
                              ast::FunctionDeclPtr function);
    static FFIValue callDecoratedFunction(const std::string& functionName, const std::vector<FFIValue>& args);
    
    static std::vector<std::string> getAvailableDecorators();
    static bool isDecorated(const std::string& functionName);

private:
    static std::unordered_map<std::string, std::string> decorators_;
    static std::unordered_map<std::string, FFIValue> decoratedFunctions_;
};

/**
 * @brief Python integration utilities
 */
class PythonUtils {
public:
    // String utilities
    static std::string escapePythonString(const std::string& str);
    static std::string unescapePythonString(const std::string& str);
    
    // Code generation utilities
    static std::string generatePythonWrapper(const std::string& functionName, 
                                            const std::vector<std::string>& paramTypes,
                                            const std::string& returnType);
    static std::string generateClassWrapper(const std::string& className,
                                          const std::vector<std::string>& methods);

    // Type conversion utilities
    static std::string tocinTypeToPython(const std::string& tocinType);
    static std::string pythonTypeToTocin(const std::string& pythonType);
    
    // Error handling utilities
    static bool isPythonException(const FFIValue& value);
    static std::string extractPythonException(const FFIValue& value);
    static FFIValue createPythonException(const std::string& exceptionType, const std::string& message);

    // Performance utilities
    static void enablePythonProfiling(bool enable);
    static std::string getPythonProfilingResults();

private:
    PythonUtils() = delete;
};

} // namespace ffi
