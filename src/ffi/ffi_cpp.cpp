#include "ffi_cpp.h"
#include <dlfcn.h>
#include <unordered_map>
#include <stdexcept>

namespace ffi {

    // Storage for loaded libraries and registered functions
    static std::unordered_map<std::string, void*> loadedLibraries;
    static std::unordered_map<std::string, void*> registeredFunctions;
    static std::string lastError;

    CppFFIImpl::CppFFIImpl() : initialized_(false) {
    }

    CppFFIImpl::~CppFFIImpl() {
        if (initialized_) {
            finalize();
        }
    }

    bool CppFFIImpl::initialize() {
        initialized_ = true;
        return true;
    }

    void CppFFIImpl::finalize() {
        // Unload all libraries
        for (auto& [name, handle] : loadedLibraries) {
            if (handle) {
                dlclose(handle);
            }
        }
        loadedLibraries.clear();
        registeredFunctions.clear();
        initialized_ = false;
    }

    std::string CppFFIImpl::getVersion() const {
        return "1.0.0";
    }

    FFIValue CppFFIImpl::callFunction(const std::string& functionName, const std::vector<FFIValue>& args) {
        auto it = registeredFunctions.find(functionName);
        if (it == registeredFunctions.end()) {
            lastError = "Function not found: " + functionName;
            return FFIValue();
        }
        // Basic implementation - would need proper type marshaling in production
        // For now, return undefined to indicate not fully implemented
        return FFIValue();
    }

    bool CppFFIImpl::hasFunction(const std::string& functionName) const {
        return registeredFunctions.find(functionName) != registeredFunctions.end();
    }

    bool CppFFIImpl::loadModule(const std::string& moduleName) {
        return loadLibrary(moduleName);
    }

    bool CppFFIImpl::unloadModule(const std::string& moduleName) {
        return unloadLibrary(moduleName);
    }

    bool CppFFIImpl::isModuleLoaded(const std::string& moduleName) const {
        return isLibraryLoaded(moduleName);
    }

    FFIValue CppFFIImpl::toFFIValue(ast::ValuePtr value) {
        // Basic conversion - would need complete implementation
        if (!value) {
            return FFIValue::createNull();
        }
        // Return undefined for now
        return FFIValue::createUndefined();
    }

    ast::ValuePtr CppFFIImpl::fromFFIValue(const FFIValue& value) {
        // Basic conversion - would need complete implementation
        return nullptr;
    }

    bool CppFFIImpl::hasError() const {
        return !lastError.empty();
    }

    std::string CppFFIImpl::getLastError() const {
        return lastError;
    }

    void CppFFIImpl::clearError() {
        lastError.clear();
    }

    std::vector<std::string> CppFFIImpl::getSupportedFeatures() const {
        return {
            "dynamic_library_loading",
            "function_calling",
            "basic_types"
        };
    }

    bool CppFFIImpl::supportsFeature(const std::string& feature) const {
        auto features = getSupportedFeatures();
        return std::find(features.begin(), features.end(), feature) != features.end();
    }

    FFIValue CppFFIImpl::eval(const std::string& code) {
        // C++ doesn't support eval natively
        lastError = "C++ FFI does not support eval";
        return FFIValue();
    }

    FFIValue CppFFIImpl::getVariable(const std::string& name) {
        // Would require symbol table implementation
        lastError = "Variable access not implemented";
        return FFIValue();
    }

    void CppFFIImpl::setVariable(const std::string& name, const FFIValue& value) {
        // Would require symbol table implementation
        lastError = "Variable setting not implemented";
    }

    bool CppFFIImpl::isAvailable() const {
        return initialized_;
    }

    // Additional CppFFIImpl methods
    bool CppFFIImpl::loadLibrary(const std::string& libraryPath) {
        if (isLibraryLoaded(libraryPath)) {
            return true;
        }

        void* handle = dlopen(libraryPath.c_str(), RTLD_LAZY);
        if (!handle) {
            lastError = std::string("Failed to load library: ") + dlerror();
            return false;
        }

        loadedLibraries[libraryPath] = handle;
        return true;
    }

    bool CppFFIImpl::unloadLibrary(const std::string& libraryPath) {
        auto it = loadedLibraries.find(libraryPath);
        if (it == loadedLibraries.end()) {
            return false;
        }

        if (it->second) {
            dlclose(it->second);
        }
        loadedLibraries.erase(it);
        return true;
    }

    bool CppFFIImpl::isLibraryLoaded(const std::string& libraryPath) const {
        return loadedLibraries.find(libraryPath) != loadedLibraries.end();
    }

    void* CppFFIImpl::getSymbol(const std::string& libraryPath, const std::string& symbolName) {
        auto it = loadedLibraries.find(libraryPath);
        if (it == loadedLibraries.end() || !it->second) {
            lastError = "Library not loaded: " + libraryPath;
            return nullptr;
        }

        void* symbol = dlsym(it->second, symbolName.c_str());
        if (!symbol) {
            lastError = std::string("Symbol not found: ") + dlerror();
            return nullptr;
        }

        return symbol;
    }

    bool CppFFIImpl::hasSymbol(const std::string& libraryPath, const std::string& symbolName) const {
        auto it = loadedLibraries.find(libraryPath);
        if (it == loadedLibraries.end() || !it->second) {
            return false;
        }

        void* symbol = dlsym(it->second, symbolName.c_str());
        return symbol != nullptr;
    }

    bool CppFFIImpl::registerFunction(const std::string& libraryPath, const std::string& functionName) {
        void* symbol = getSymbol(libraryPath, functionName);
        if (!symbol) {
            return false;
        }

        registeredFunctions[functionName] = symbol;
        return true;
    }

    FFIValue CppFFIImpl::callFunctionPtr(void* functionPtr, const std::string& functionName, 
                                        const std::vector<FFIValue>& args) {
        // Basic implementation - full implementation would require libffi or similar
        if (!functionPtr) {
            lastError = "Invalid function pointer";
            return FFIValue();
        }
        // Placeholder - actual calling would require type marshaling
        lastError = "Function calling not fully implemented";
        return FFIValue();
    }

    bool CppFFIImpl::registerClass(const std::string& libraryPath, const std::string& className) {
        // Class registration would require additional metadata
        lastError = "Class registration not implemented";
        return false;
    }

    FFIValue CppFFIImpl::createInstance(const std::string& className, const std::vector<FFIValue>& constructorArgs) {
        lastError = "Instance creation not implemented";
        return FFIValue();
    }

    bool CppFFIImpl::destroyInstance(const std::string& className, FFIValue& instance) {
        lastError = "Instance destruction not implemented";
        return false;
    }

    FFIValue CppFFIImpl::getMember(const FFIValue& instance, const std::string& memberName) {
        lastError = "Member access not implemented";
        return FFIValue();
    }

    bool CppFFIImpl::setMember(FFIValue& instance, const std::string& memberName, const FFIValue& value) {
        lastError = "Member setting not implemented";
        return false;
    }

    FFIValue CppFFIImpl::callMethod(FFIValue& instance, const std::string& methodName, const std::vector<FFIValue>& args) {
        lastError = "Method calling not implemented";
        return FFIValue();
    }

    FFIValue CppFFIImpl::callStaticMethod(const std::string& className, const std::string& methodName, 
                                         const std::vector<FFIValue>& args) {
        lastError = "Static method calling not implemented";
        return FFIValue();
    }

    bool CppFFIImpl::registerTemplate(const std::string& libraryPath, const std::string& templateName) {
        lastError = "Template registration not implemented";
        return false;
    }

    std::string CppFFIImpl::instantiateTemplate(const std::string& templateName, const std::vector<std::string>& typeArgs) {
        lastError = "Template instantiation not implemented";
        return "";
    }

    FFIValue CppFFIImpl::createVector(const std::string& elementType, const std::vector<FFIValue>& elements) {
        // Basic vector creation - convert to FFI array
        return FFIValue(elements);
    }

    FFIValue CppFFIImpl::createMap(const std::string& keyType, const std::string& valueType, 
                                  const std::vector<std::pair<FFIValue, FFIValue>>& pairs) {
        // Basic map creation - convert to FFI object
        std::unordered_map<std::string, FFIValue> map;
        for (const auto& [key, value] : pairs) {
            if (key.isString()) {
                map[key.asString()] = value;
            }
        }
        return FFIValue(map);
    }

    FFIValue CppFFIImpl::createSet(const std::string& elementType, const std::vector<FFIValue>& elements) {
        // Basic set creation - convert to FFI array (sets are like arrays but with unique elements)
        return FFIValue(elements);
    }

    FFIValue CppFFIImpl::allocateMemory(size_t size, const std::string& typeName) {
        void* ptr = malloc(size);
        if (!ptr) {
            lastError = "Memory allocation failed";
            return FFIValue();
        }
        return FFIValue(ptr, typeName);
    }

    bool CppFFIImpl::deallocateMemory(FFIValue& value) {
        if (value.isPointer()) {
            free(value.asPointer());
            return true;
        }
        return false;
    }

    FFIValue CppFFIImpl::createReference(FFIValue& value) {
        // Create a reference by wrapping the pointer to the value
        return FFIValue(static_cast<void*>(&value), "reference");
    }

    FFIValue CppFFIImpl::dereference(const FFIValue& pointer) {
        if (!pointer.isPointer()) {
            lastError = "Cannot dereference non-pointer value";
            return FFIValue();
        }
        // Basic dereferencing - would need type information in production
        return FFIValue();
    }

    bool CppFFIImpl::hasException() const {
        return hasError();
    }

    std::string CppFFIImpl::getLastException() const {
        return getLastError();
    }

    void CppFFIImpl::clearException() {
        clearError();
    }

    FFIValue CppFFIImpl::ffiValueToCpp(const FFIValue& value) {
        // Identity conversion - FFIValue is already C++ compatible
        return value;
    }

    FFIValue CppFFIImpl::cppValueToFFI(const FFIValue& value) {
        // Identity conversion - FFIValue is already C++ compatible
        return value;
    }

} // namespace ffi
