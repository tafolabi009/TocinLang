#include "ffi_cpp.h"
#include <dlfcn.h>
#include <ffi.h>
#include <unordered_map>
#include <stdexcept>
#include <cstring>

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
        
        // Call the function using the registered function pointer
        return callFunctionPtr(it->second, functionName, args);
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
        if (!functionPtr) {
            lastError = "Invalid function pointer";
            return FFIValue();
        }
        
        // Use libffi for dynamic function calling
        ffi_cif cif;
        ffi_type **arg_types = nullptr;
        void **arg_values = nullptr;
        ffi_type *return_type = &ffi_type_pointer; // Default to pointer
        void *return_value = nullptr;
        
        try {
            // Allocate argument type and value arrays
            size_t arg_count = args.size();
            arg_types = new ffi_type*[arg_count];
            arg_values = new void*[arg_count];
            
            // Convert FFIValue arguments to libffi format
            std::vector<int32_t> int_storage;
            std::vector<int64_t> long_storage;
            std::vector<double> double_storage;
            std::vector<void*> ptr_storage;
            
            for (size_t i = 0; i < arg_count; ++i) {
                const auto& arg = args[i];
                
                switch (arg.getType()) {
                    case FFIValue::Type::BOOLEAN:
                        arg_types[i] = &ffi_type_sint32;
                        int_storage.push_back(arg.asBoolean() ? 1 : 0);
                        arg_values[i] = &int_storage.back();
                        break;
                        
                    case FFIValue::Type::INTEGER:
                        if (arg.asInt64() >= INT32_MIN && arg.asInt64() <= INT32_MAX) {
                            arg_types[i] = &ffi_type_sint32;
                            int_storage.push_back(static_cast<int32_t>(arg.asInt64()));
                            arg_values[i] = &int_storage.back();
                        } else {
                            arg_types[i] = &ffi_type_sint64;
                            long_storage.push_back(arg.asInt64());
                            arg_values[i] = &long_storage.back();
                        }
                        break;
                        
                    case FFIValue::Type::FLOAT:
                        arg_types[i] = &ffi_type_double;
                        double_storage.push_back(arg.asDouble());
                        arg_values[i] = &double_storage.back();
                        break;
                        
                    case FFIValue::Type::STRING:
                        arg_types[i] = &ffi_type_pointer;
                        ptr_storage.push_back(const_cast<char*>(arg.asString().c_str()));
                        arg_values[i] = &ptr_storage.back();
                        break;
                        
                    case FFIValue::Type::POINTER:
                        arg_types[i] = &ffi_type_pointer;
                        ptr_storage.push_back(arg.asPointer());
                        arg_values[i] = &ptr_storage.back();
                        break;
                        
                    default:
                        // For unsupported types, pass as NULL pointer
                        arg_types[i] = &ffi_type_pointer;
                        ptr_storage.push_back(nullptr);
                        arg_values[i] = &ptr_storage.back();
                        break;
                }
            }
            
            // Prepare the CIF (Call InterFace)
            if (ffi_prep_cif(&cif, FFI_DEFAULT_ABI, arg_count, return_type, arg_types) != FFI_OK) {
                lastError = "Failed to prepare FFI call interface";
                delete[] arg_types;
                delete[] arg_values;
                return FFIValue();
            }
            
            // Allocate return value storage
            return_value = malloc(sizeof(void*));
            
            // Make the call
            ffi_call(&cif, FFI_FN(functionPtr), return_value, arg_values);
            
            // Convert return value (assume pointer for now, would need type info for proper conversion)
            FFIValue result;
            if (return_value) {
                void* ret_ptr = *static_cast<void**>(return_value);
                if (ret_ptr) {
                    result = FFIValue(ret_ptr, "unknown");
                }
            }
            
            // Cleanup
            free(return_value);
            delete[] arg_types;
            delete[] arg_values;
            
            return result;
            
        } catch (const std::exception& e) {
            lastError = std::string("FFI call failed: ") + e.what();
            if (arg_types) delete[] arg_types;
            if (arg_values) delete[] arg_values;
            if (return_value) free(return_value);
            return FFIValue();
        }
    }

    bool CppFFIImpl::registerClass(const std::string& libraryPath, const std::string& className) {
        // Load the library if not already loaded
        if (!isLibraryLoaded(libraryPath)) {
            if (!loadLibrary(libraryPath)) {
                return false;
            }
        }
        
        // Get class metadata - constructor, destructor, methods
        std::string ctorName = className + "_create";
        std::string dtorName = className + "_destroy";
        
        void* ctorSym = getSymbol(libraryPath, ctorName);
        void* dtorSym = getSymbol(libraryPath, dtorName);
        
        if (!ctorSym || !dtorSym) {
            lastError = "Class must export " + ctorName + " and " + dtorName + " functions";
            return false;
        }
        
        // Store class metadata
        registeredFunctions[className + "::__ctor"] = ctorSym;
        registeredFunctions[className + "::__dtor"] = dtorSym;
        
        return true;
    }

    FFIValue CppFFIImpl::createInstance(const std::string& className, const std::vector<FFIValue>& constructorArgs) {
        auto ctorIt = registeredFunctions.find(className + "::__ctor");
        if (ctorIt == registeredFunctions.end()) {
            lastError = "Class not registered: " + className;
            return FFIValue();
        }
        
        // Call constructor function pointer
        FFIValue instance = callFunctionPtr(ctorIt->second, className + "::__ctor", constructorArgs);
        
        if (instance.isPointer()) {
            // Tag the instance with its class name for later method dispatch
            instance.setMetadata("__class", className);
        }
        
        return instance;
    }

    bool CppFFIImpl::destroyInstance(const std::string& className, FFIValue& instance) {
        if (!instance.isPointer()) {
            lastError = "Cannot destroy non-pointer instance";
            return false;
        }
        
        auto dtorIt = registeredFunctions.find(className + "::__dtor");
        if (dtorIt == registeredFunctions.end()) {
            lastError = "Class not registered: " + className;
            return false;
        }
        
        // Call destructor
        callFunctionPtr(dtorIt->second, className + "::__dtor", {instance});
        
        // Clear the instance
        instance = FFIValue();
        return true;
    }

    FFIValue CppFFIImpl::getMember(const FFIValue& instance, const std::string& memberName) {
        if (!instance.isPointer()) {
            lastError = "Cannot get member of non-pointer instance";
            return FFIValue();
        }
        
        std::string className = instance.getMetadata("__class");
        if (className.empty()) {
            lastError = "Instance does not have class metadata";
            return FFIValue();
        }
        
        // Generate getter function name: ClassName_get_memberName
        std::string getterName = className + "_get_" + memberName;
        
        auto it = registeredFunctions.find(getterName);
        if (it != registeredFunctions.end()) {
            return callFunctionPtr(it->second, getterName, {instance});
        }
        
        // Try direct memory access (unsafe, but works for POD types)
        // This requires metadata about member offsets which we don't have here
        lastError = "Member getter not found: " + getterName;
        return FFIValue();
    }

    bool CppFFIImpl::setMember(FFIValue& instance, const std::string& memberName, const FFIValue& value) {
        if (!instance.isPointer()) {
            lastError = "Cannot set member of non-pointer instance";
            return false;
        }
        
        std::string className = instance.getMetadata("__class");
        if (className.empty()) {
            lastError = "Instance does not have class metadata";
            return false;
        }
        
        // Generate setter function name: ClassName_set_memberName
        std::string setterName = className + "_set_" + memberName;
        
        auto it = registeredFunctions.find(setterName);
        if (it != registeredFunctions.end()) {
            callFunctionPtr(it->second, setterName, {instance, value});
            return true;
        }
        
        lastError = "Member setter not found: " + setterName;
        return false;
    }

    FFIValue CppFFIImpl::callMethod(FFIValue& instance, const std::string& methodName, 
                                   const std::vector<FFIValue>& args) {
        if (!instance.isPointer()) {
            lastError = "Cannot call method on non-pointer instance";
            return FFIValue();
        }
        
        std::string className = instance.getMetadata("__class");
        if (className.empty()) {
            lastError = "Instance does not have class metadata";
            return FFIValue();
        }
        
        // Generate method function name: ClassName_methodName
        std::string fullMethodName = className + "_" + methodName;
        
        auto it = registeredFunctions.find(fullMethodName);
        if (it == registeredFunctions.end()) {
            lastError = "Method not found: " + fullMethodName;
            return FFIValue();
        }
        
        // Prepend instance to arguments (this pointer)
        std::vector<FFIValue> allArgs = {instance};
        allArgs.insert(allArgs.end(), args.begin(), args.end());
        
        return callFunctionPtr(it->second, fullMethodName, allArgs);
    }

    FFIValue CppFFIImpl::callStaticMethod(const std::string& className, const std::string& methodName, 
                                         const std::vector<FFIValue>& args) {
        // Generate static method function name: ClassName_static_methodName
        std::string fullMethodName = className + "_static_" + methodName;
        
        auto it = registeredFunctions.find(fullMethodName);
        if (it == registeredFunctions.end()) {
            lastError = "Static method not found: " + fullMethodName;
            return FFIValue();
        }
        
        return callFunctionPtr(it->second, fullMethodName, args);
    }

    bool CppFFIImpl::registerTemplate(const std::string& libraryPath, const std::string& templateName) {
        // Templates in C++ need compile-time instantiation
        // We support runtime instantiation through pre-compiled template instances
        
        if (!isLibraryLoaded(libraryPath)) {
            if (!loadLibrary(libraryPath)) {
                return false;
            }
        }
        
        // Register the template metadata
        // Expect symbol: templateName_instantiate
        std::string instantiateFuncName = templateName + "_instantiate";
        
        void* instantiateFunc = getSymbol(libraryPath, instantiateFuncName);
        if (!instantiateFunc) {
            lastError = "Template must export " + instantiateFuncName + " function";
            return false;
        }
        
        registeredFunctions["template::" + templateName + "::instantiate"] = instantiateFunc;
        return true;
    }

    std::string CppFFIImpl::instantiateTemplate(const std::string& templateName, const std::vector<std::string>& typeArgs) {
        auto it = registeredFunctions.find("template::" + templateName + "::instantiate");
        if (it == registeredFunctions.end()) {
            lastError = "Template not registered: " + templateName;
            return "";
        }
        
        // Generate mangled name for this instantiation
        std::string instantiatedName = templateName + "<";
        for (size_t i = 0; i < typeArgs.size(); ++i) {
            if (i > 0) instantiatedName += ",";
            instantiatedName += typeArgs[i];
        }
        instantiatedName += ">";
        
        // Call template instantiation function
        // This would typically trigger dynamic code generation or load a pre-compiled instance
        std::vector<FFIValue> args;
        for (const auto& type : typeArgs) {
            args.push_back(FFIValue(type));
        }
        
        FFIValue result = callFunctionPtr(it->second, templateName + "::instantiate", args);
        
        if (result.isString()) {
            return result.asString();
        }
        
        return instantiatedName;
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

// ============================================================================
// CppLibrary Implementation
// ============================================================================

CppLibrary::CppLibrary(const std::string& path, CppFFIImpl* ffi)
    : path_(path), ffi_(ffi), loaded_(false), handle_(nullptr) {}

CppLibrary::~CppLibrary() {
    if (loaded_) {
        unload();
    }
}

bool CppLibrary::load() {
    if (loaded_) return true;
    
    loaded_ = ffi_->loadLibrary(path_);
    if (loaded_) {
        // Get the actual handle from the FFI implementation
        handle_ = const_cast<void*>(
            static_cast<const void*>(loadedLibraries[path_])
        );
    }
    return loaded_;
}

bool CppLibrary::unload() {
    if (!loaded_) return true;
    
    bool result = ffi_->unloadLibrary(path_);
    if (result) {
        loaded_ = false;
        handle_ = nullptr;
    }
    return result;
}

void* CppLibrary::getSymbol(const std::string& symbolName) {
    if (!loaded_) return nullptr;
    return ffi_->getSymbol(path_, symbolName);
}

bool CppLibrary::hasSymbol(const std::string& symbolName) const {
    if (!loaded_) return false;
    return ffi_->hasSymbol(path_, symbolName);
}

bool CppLibrary::registerFunction(const std::string& functionName) {
    if (!loaded_) return false;
    return ffi_->registerFunction(path_, functionName);
}

bool CppLibrary::registerClass(const std::string& className) {
    if (!loaded_) return false;
    return ffi_->registerClass(path_, className);
}

bool CppLibrary::registerTemplate(const std::string& templateName) {
    if (!loaded_) return false;
    return ffi_->registerTemplate(path_, templateName);
}

// ============================================================================
// CppClass Implementation
// ============================================================================

CppClass::CppClass(const std::string& className, const std::string& libraryPath, CppFFIImpl* ffi)
    : className_(className), libraryPath_(libraryPath), ffi_(ffi) {}

CppClass::~CppClass() {}

FFIValue CppClass::createInstance(const std::vector<FFIValue>& constructorArgs) {
    return ffi_->createInstance(className_, constructorArgs);
}

bool CppClass::destroyInstance(FFIValue& instance) {
    return ffi_->destroyInstance(className_, instance);
}

FFIValue CppClass::callStaticMethod(const std::string& methodName, const std::vector<FFIValue>& args) {
    return ffi_->callStaticMethod(className_, methodName, args);
}

bool CppClass::hasMethod(const std::string& methodName) const {
    std::string fullName = className_ + "_" + methodName;
    return ffi_->hasFunction(fullName);
}

bool CppClass::hasStaticMethod(const std::string& methodName) const {
    std::string fullName = className_ + "_static_" + methodName;
    return ffi_->hasFunction(fullName);
}

bool CppClass::hasMember(const std::string& memberName) const {
    std::string getterName = className_ + "_get_" + memberName;
    return ffi_->hasFunction(getterName);
}

// ============================================================================
// CppInstance Implementation
// ============================================================================

CppInstance::CppInstance(const FFIValue& instance, const std::string& className, CppFFIImpl* ffi)
    : instance_(instance), className_(className), ffi_(ffi) {}

CppInstance::~CppInstance() {}

FFIValue CppInstance::callMethod(const std::string& methodName, const std::vector<FFIValue>& args) {
    return ffi_->callMethod(const_cast<FFIValue&>(instance_), methodName, args);
}

FFIValue CppInstance::getMember(const std::string& memberName) {
    return ffi_->getMember(instance_, memberName);
}

bool CppInstance::setMember(const std::string& memberName, const FFIValue& value) {
    return ffi_->setMember(const_cast<FFIValue&>(instance_), memberName, value);
}

bool CppInstance::hasMethod(const std::string& methodName) const {
    std::string fullName = className_ + "_" + methodName;
    return ffi_->hasFunction(fullName);
}

bool CppInstance::hasMember(const std::string& memberName) const {
    std::string getterName = className_ + "_get_" + memberName;
    return ffi_->hasFunction(getterName);
}

// ============================================================================
// CppTemplate Implementation
// ============================================================================

CppTemplate::CppTemplate(const std::string& templateName, const std::string& libraryPath, CppFFIImpl* ffi)
    : templateName_(templateName), libraryPath_(libraryPath), ffi_(ffi) {}

CppTemplate::~CppTemplate() {}

std::string CppTemplate::instantiate(const std::vector<std::string>& typeArgs) {
    return ffi_->instantiateTemplate(templateName_, typeArgs);
}

bool CppTemplate::hasInstantiation(const std::vector<std::string>& typeArgs) const {
    // Check if this template instantiation exists
    std::string mangledName = templateName_ + "<";
    for (size_t i = 0; i < typeArgs.size(); ++i) {
        if (i > 0) mangledName += ",";
        mangledName += typeArgs[i];
    }
    mangledName += ">";
    
    return ffi_->hasFunction(mangledName);
}

std::vector<std::string> CppTemplate::getTypeParameters() const {
    // This would require parsing template metadata
    // For now, return empty vector
    return {};
}

// ============================================================================
// CppStdLib Implementation
// ============================================================================

bool CppStdLib::initialized_ = false;
CppFFIImpl* CppStdLib::ffi_ = nullptr;

bool CppStdLib::initialize(CppFFIImpl* ffi) {
    ffi_ = ffi;
    initialized_ = true;
    return true;
}

bool CppStdLib::isAvailable() {
    return initialized_ && ffi_ != nullptr;
}

FFIValue CppStdLib::createString(const std::string& value) {
    return FFIValue(value);
}

std::string CppStdLib::getString(const FFIValue& cppString) {
    if (cppString.isString()) {
        return cppString.asString();
    }
    return "";
}

FFIValue CppStdLib::stringConcat(const FFIValue& a, const FFIValue& b) {
    if (a.isString() && b.isString()) {
        return FFIValue(a.asString() + b.asString());
    }
    return FFIValue();
}

size_t CppStdLib::stringLength(const FFIValue& cppString) {
    if (cppString.isString()) {
        return cppString.asString().length();
    }
    return 0;
}

FFIValue CppStdLib::createIntVector(const std::vector<int>& values) {
    std::vector<FFIValue> ffiValues;
    for (int v : values) {
        ffiValues.push_back(FFIValue(static_cast<int64_t>(v)));
    }
    return FFIValue(ffiValues);
}

FFIValue CppStdLib::createDoubleVector(const std::vector<double>& values) {
    std::vector<FFIValue> ffiValues;
    for (double v : values) {
        ffiValues.push_back(FFIValue(v));
    }
    return FFIValue(ffiValues);
}

FFIValue CppStdLib::createStringVector(const std::vector<std::string>& values) {
    std::vector<FFIValue> ffiValues;
    for (const auto& v : values) {
        ffiValues.push_back(FFIValue(v));
    }
    return FFIValue(ffiValues);
}

std::vector<int> CppStdLib::getIntVector(const FFIValue& cppVector) {
    std::vector<int> result;
    if (cppVector.isArray()) {
        auto arr = cppVector.asArray();
        for (const auto& v : arr) {
            if (v.isInteger()) {
                result.push_back(static_cast<int>(v.asInt64()));
            }
        }
    }
    return result;
}

std::vector<double> CppStdLib::getDoubleVector(const FFIValue& cppVector) {
    std::vector<double> result;
    if (cppVector.isArray()) {
        auto arr = cppVector.asArray();
        for (const auto& v : arr) {
            if (v.isFloat()) {
                result.push_back(v.asDouble());
            }
        }
    }
    return result;
}

std::vector<std::string> CppStdLib::getStringVector(const FFIValue& cppVector) {
    std::vector<std::string> result;
    if (cppVector.isArray()) {
        auto arr = cppVector.asArray();
        for (const auto& v : arr) {
            if (v.isString()) {
                result.push_back(v.asString());
            }
        }
    }
    return result;
}

FFIValue CppStdLib::createStringIntMap(const std::unordered_map<std::string, int>& values) {
    std::unordered_map<std::string, FFIValue> ffiMap;
    for (const auto& [k, v] : values) {
        ffiMap[k] = FFIValue(static_cast<int64_t>(v));
    }
    return FFIValue(ffiMap);
}

FFIValue CppStdLib::createStringDoubleMap(const std::unordered_map<std::string, double>& values) {
    std::unordered_map<std::string, FFIValue> ffiMap;
    for (const auto& [k, v] : values) {
        ffiMap[k] = FFIValue(v);
    }
    return FFIValue(ffiMap);
}

std::unordered_map<std::string, int> CppStdLib::getStringIntMap(const FFIValue& cppMap) {
    std::unordered_map<std::string, int> result;
    if (cppMap.isObject()) {
        auto obj = cppMap.asObject();
        for (const auto& [k, v] : obj) {
            if (v.isInteger()) {
                result[k] = static_cast<int>(v.asInt64());
            }
        }
    }
    return result;
}

std::unordered_map<std::string, double> CppStdLib::getStringDoubleMap(const FFIValue& cppMap) {
    std::unordered_map<std::string, double> result;
    if (cppMap.isObject()) {
        auto obj = cppMap.asObject();
        for (const auto& [k, v] : obj) {
            if (v.isFloat()) {
                result[k] = v.asDouble();
            }
        }
    }
    return result;
}

FFIValue CppStdLib::createIntSet(const std::vector<int>& values) {
    return createIntVector(values);
}

FFIValue CppStdLib::createStringSet(const std::vector<std::string>& values) {
    return createStringVector(values);
}

std::vector<int> CppStdLib::getIntSet(const FFIValue& cppSet) {
    return getIntVector(cppSet);
}

std::vector<std::string> CppStdLib::getStringSet(const FFIValue& cppSet) {
    return getStringVector(cppSet);
}

FFIValue CppStdLib::sort(const FFIValue& container) {
    if (!container.isArray()) {
        return container;
    }
    
    auto arr = container.asArray();
    std::sort(arr.begin(), arr.end(), [](const FFIValue& a, const FFIValue& b) {
        if (a.isInteger() && b.isInteger()) {
            return a.asInt64() < b.asInt64();
        }
        if (a.isFloat() && b.isFloat()) {
            return a.asDouble() < b.asDouble();
        }
        if (a.isString() && b.isString()) {
            return a.asString() < b.asString();
        }
        return false;
    });
    
    return FFIValue(arr);
}

FFIValue CppStdLib::find(const FFIValue& container, const FFIValue& value) {
    if (!container.isArray()) {
        return FFIValue();
    }
    
    auto arr = container.asArray();
    for (size_t i = 0; i < arr.size(); ++i) {
        // Simple equality check
        if (arr[i].getType() == value.getType()) {
            bool equal = false;
            
            if (arr[i].isInteger() && value.isInteger()) {
                equal = arr[i].asInt64() == value.asInt64();
            } else if (arr[i].isFloat() && value.isFloat()) {
                equal = arr[i].asDouble() == value.asDouble();
            } else if (arr[i].isString() && value.isString()) {
                equal = arr[i].asString() == value.asString();
            }
            
            if (equal) {
                return FFIValue(static_cast<int64_t>(i));
            }
        }
    }
    
    return FFIValue(); // Not found
}

FFIValue CppStdLib::transform(const FFIValue& container, void* transformFunc) {
    // This would require function pointer invocation
    // For now, return the container as-is
    return container;
}

// ============================================================================
// CppUtils Implementation
// ============================================================================

std::string CppUtils::mangleFunctionName(const std::string& functionName, 
                                        const std::vector<std::string>& paramTypes) {
    // Simplified Itanium C++ ABI name mangling
    std::string mangled = "_Z";
    mangled += std::to_string(functionName.length());
    mangled += functionName;
    
    for (const auto& type : paramTypes) {
        if (type == "int") mangled += "i";
        else if (type == "long") mangled += "l";
        else if (type == "float") mangled += "f";
        else if (type == "double") mangled += "d";
        else if (type == "bool") mangled += "b";
        else if (type == "char") mangled += "c";
        else if (type == "void") mangled += "v";
        else {
            // Complex type - use length + name
            mangled += std::to_string(type.length());
            mangled += type;
        }
    }
    
    return mangled;
}

std::string CppUtils::demangleFunctionName(const std::string& mangledName) {
    // This would require a full demangler implementation
    // For now, return the mangled name
    return mangledName;
}

std::string CppUtils::tocinTypeToCpp(const std::string& tocinType) {
    if (tocinType == "int") return "int";
    if (tocinType == "int64") return "long long";
    if (tocinType == "float") return "float";
    if (tocinType == "float64") return "double";
    if (tocinType == "bool") return "bool";
    if (tocinType == "string") return "std::string";
    if (tocinType == "void") return "void";
    return tocinType;
}

std::string CppUtils::cppTypeToTocin(const std::string& cppType) {
    if (cppType == "int") return "int";
    if (cppType == "long long") return "int64";
    if (cppType == "float") return "float";
    if (cppType == "double") return "float64";
    if (cppType == "bool") return "bool";
    if (cppType == "std::string") return "string";
    if (cppType == "void") return "void";
    return cppType;
}

size_t CppUtils::getTypeSize(const std::string& cppType) {
    if (cppType == "char") return sizeof(char);
    if (cppType == "short") return sizeof(short);
    if (cppType == "int") return sizeof(int);
    if (cppType == "long") return sizeof(long);
    if (cppType == "long long") return sizeof(long long);
    if (cppType == "float") return sizeof(float);
    if (cppType == "double") return sizeof(double);
    if (cppType == "bool") return sizeof(bool);
    if (cppType.find("*") != std::string::npos) return sizeof(void*);
    return 0; // Unknown size
}

std::string CppUtils::generateCppWrapper(const std::string& functionName, 
                                        const std::vector<std::string>& paramTypes,
                                        const std::string& returnType) {
    std::string wrapper = "extern \"C\" " + tocinTypeToCpp(returnType) + " ";
    wrapper += functionName + "_wrapper(";
    
    for (size_t i = 0; i < paramTypes.size(); ++i) {
        if (i > 0) wrapper += ", ";
        wrapper += tocinTypeToCpp(paramTypes[i]) + " arg" + std::to_string(i);
    }
    
    wrapper += ") {\n";
    wrapper += "    return " + functionName + "(";
    
    for (size_t i = 0; i < paramTypes.size(); ++i) {
        if (i > 0) wrapper += ", ";
        wrapper += "arg" + std::to_string(i);
    }
    
    wrapper += ");\n";
    wrapper += "}\n";
    
    return wrapper;
}

std::string CppUtils::generateClassWrapper(const std::string& className,
                                          const std::vector<std::string>& methods) {
    std::string wrapper = "// Wrapper for class " + className + "\n\n";
    
    // Constructor wrapper
    wrapper += "extern \"C\" void* " + className + "_create() {\n";
    wrapper += "    return new " + className + "();\n";
    wrapper += "}\n\n";
    
    // Destructor wrapper
    wrapper += "extern \"C\" void " + className + "_destroy(void* obj) {\n";
    wrapper += "    delete static_cast<" + className + "*>(obj);\n";
    wrapper += "}\n\n";
    
    // Method wrappers
    for (const auto& method : methods) {
        wrapper += "extern \"C\" void " + className + "_" + method + "(void* obj) {\n";
        wrapper += "    static_cast<" + className + "*>(obj)->" + method + "();\n";
        wrapper += "}\n\n";
    }
    
    return wrapper;
}

bool CppUtils::isCppException(const FFIValue& value) {
    // Check if this is an exception object
    return value.isObject() && value.hasMetadata("__exception");
}

std::string CppUtils::extractCppException(const FFIValue& value) {
    if (isCppException(value)) {
        return value.getMetadata("__exception_message");
    }
    return "";
}

FFIValue CppUtils::createCppException(const std::string& exceptionType, const std::string& message) {
    FFIValue exception;
    exception.setMetadata("__exception", "true");
    exception.setMetadata("__exception_type", exceptionType);
    exception.setMetadata("__exception_message", message);
    return exception;
}

size_t CppUtils::getObjectSize(const FFIValue& object) {
    // This would require runtime type information
    return 0;
}

bool CppUtils::isValidPointer(const FFIValue& pointer) {
    return pointer.isPointer() && pointer.asPointer() != nullptr;
}

FFIValue CppUtils::cloneObject(const FFIValue& object) {
    // Deep copy of FFIValue
    return object;
}

void CppUtils::enableCppProfiling(bool enable) {
    // Profiling enablement would be implementation-specific
}

std::string CppUtils::getCppProfilingResults() {
    // Return profiling results
    return "Profiling not yet implemented";
}

} // namespace ffi

