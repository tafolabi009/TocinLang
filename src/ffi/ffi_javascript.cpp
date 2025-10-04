#include "ffi_javascript.h"
#include <stdexcept>
#include <sstream>
#include <algorithm>

namespace ffi {

    // Internal state for JavaScript FFI - defined in cpp to avoid header dependency
    struct JavaScriptFFIImpl::JSInternalState {
        bool initialized;
        std::string lastError;
        std::unordered_map<std::string, bool> loadedModules;
        std::unordered_map<std::string, FFIValue> globalVariables;
        std::unordered_map<std::string, FFIValue> registeredFunctions;
    };

    JavaScriptFFIImpl::JavaScriptFFIImpl() : initialized_(false) {
        state_ = std::make_unique<JSInternalState>();
        state_->initialized = false;
    }

    JavaScriptFFIImpl::~JavaScriptFFIImpl() {
        finalize();
    }

    bool JavaScriptFFIImpl::initialize() {
        if (!state_) {
            state_ = std::make_unique<JSInternalState>();
        }
        state_->initialized = true;
        initialized_ = true;
        state_->lastError.clear();
        return true;
    }

    void JavaScriptFFIImpl::finalize() {
        if (state_) {
            state_->initialized = false;
            state_->loadedModules.clear();
            state_->globalVariables.clear();
            state_->registeredFunctions.clear();
        }
        initialized_ = false;
    }

    std::string JavaScriptFFIImpl::getVersion() const {
        return "1.0.0-stub";
    }

    FFIValue JavaScriptFFIImpl::callFunction(const std::string& functionName, const std::vector<FFIValue>& args) {
        if (!state_ || !state_->initialized) {
            state_->lastError = "JavaScript FFI not initialized";
            return FFIValue();
        }
        
        // Check if function is registered
        auto it = state_->registeredFunctions.find(functionName);
        if (it != state_->registeredFunctions.end()) {
            return it->second; // Return stored function result
        }
        
        state_->lastError = "Function not found: " + functionName;
        return FFIValue();
    }

    bool JavaScriptFFIImpl::hasFunction(const std::string& functionName) const {
        if (!state_ || !state_->initialized) {
            return false;
        }
        return state_->registeredFunctions.find(functionName) != state_->registeredFunctions.end();
    }

    bool JavaScriptFFIImpl::loadModule(const std::string& moduleName) {
        if (!state_ || !state_->initialized) {
            state_->lastError = "JavaScript FFI not initialized";
            return false;
        }
        state_->loadedModules[moduleName] = true;
        return true;
    }

    bool JavaScriptFFIImpl::unloadModule(const std::string& moduleName) {
        if (!state_ || !state_->initialized) {
            return false;
        }
        auto it = state_->loadedModules.find(moduleName);
        if (it != state_->loadedModules.end()) {
            state_->loadedModules.erase(it);
            return true;
        }
        return false;
    }

    bool JavaScriptFFIImpl::isModuleLoaded(const std::string& moduleName) const {
        if (!state_ || !state_->initialized) {
            return false;
        }
        auto it = state_->loadedModules.find(moduleName);
        return it != state_->loadedModules.end() && it->second;
    }

    FFIValue JavaScriptFFIImpl::toFFIValue(ast::ValuePtr value) {
        // Simple conversion for basic types
        if (!value) {
            return FFIValue();
        }
        // Basic stub implementation - would need full AST value handling
        return FFIValue();
    }

    ast::ValuePtr JavaScriptFFIImpl::fromFFIValue(const FFIValue& value) {
        // Basic stub implementation - would need full AST value construction
        return nullptr;
    }

    bool JavaScriptFFIImpl::hasError() const {
        return state_ && !state_->lastError.empty();
    }

    std::string JavaScriptFFIImpl::getLastError() const {
        return state_ ? state_->lastError : "JavaScript FFI not initialized";
    }

    void JavaScriptFFIImpl::clearError() {
        if (state_) {
            state_->lastError.clear();
        }
    }

    std::vector<std::string> JavaScriptFFIImpl::getSupportedFeatures() const {
        return {"eval", "modules", "objects", "arrays", "promises"};
    }

    bool JavaScriptFFIImpl::supportsFeature(const std::string& feature) const {
        auto features = getSupportedFeatures();
        return std::find(features.begin(), features.end(), feature) != features.end();
    }

    FFIValue JavaScriptFFIImpl::eval(const std::string& code) {
        if (!state_ || !state_->initialized) {
            state_->lastError = "JavaScript FFI not initialized";
            return FFIValue();
        }
        // Stub: would need V8 or similar JavaScript engine
        state_->lastError = "JavaScript eval not fully implemented - requires V8 integration";
        return FFIValue();
    }

    FFIValue JavaScriptFFIImpl::getVariable(const std::string& name) {
        if (!state_ || !state_->initialized) {
            return FFIValue();
        }
        auto it = state_->globalVariables.find(name);
        if (it != state_->globalVariables.end()) {
            return it->second;
        }
        return FFIValue();
    }

    void JavaScriptFFIImpl::setVariable(const std::string& name, const FFIValue& value) {
        if (state_ && state_->initialized) {
            state_->globalVariables[name] = value;
        }
    }

    bool JavaScriptFFIImpl::isAvailable() const {
        return initialized_ && state_ && state_->initialized;
    }

    // JavaScript-specific methods
    FFIValue JavaScriptFFIImpl::executeCode(const std::string& code) {
        return eval(code);
    }

    FFIValue JavaScriptFFIImpl::executeFile(const std::string& filename) {
        if (!state_ || !state_->initialized) {
            state_->lastError = "JavaScript FFI not initialized";
            return FFIValue();
        }
        // Stub: would need to read file and execute
        state_->lastError = "File execution not fully implemented - requires V8 integration";
        return FFIValue();
    }

    bool JavaScriptFFIImpl::loadModuleFromCode(const std::string& moduleName, const std::string& code) {
        if (!state_ || !state_->initialized) {
            return false;
        }
        // Register the module as loaded
        state_->loadedModules[moduleName] = true;
        return true;
    }

    FFIValue JavaScriptFFIImpl::callMethod(const FFIValue& object, const std::string& methodName, const std::vector<FFIValue>& args) {
        if (!state_ || !state_->initialized) {
            state_->lastError = "JavaScript FFI not initialized";
            return FFIValue();
        }
        // Stub: would need V8 object method calling
        state_->lastError = "Method calling not fully implemented - requires V8 integration";
        return FFIValue();
    }

    FFIValue JavaScriptFFIImpl::createObject(const std::unordered_map<std::string, FFIValue>& properties) {
        // Create an object FFIValue from properties
        return FFIValue(properties);
    }

    FFIValue JavaScriptFFIImpl::getProperty(const FFIValue& object, const std::string& propertyName) {
        if (!object.isObject()) {
            return FFIValue();
        }
        auto& obj = object.asObject();
        auto it = obj.find(propertyName);
        if (it != obj.end()) {
            return it->second;
        }
        return FFIValue();
    }

    bool JavaScriptFFIImpl::setProperty(FFIValue& object, const std::string& propertyName, const FFIValue& value) {
        if (!object.isObject()) {
            return false;
        }
        auto& obj = object.asObject();
        obj[propertyName] = value;
        return true;
    }

    bool JavaScriptFFIImpl::hasProperty(const FFIValue& object, const std::string& propertyName) {
        if (!object.isObject()) {
            return false;
        }
        const auto& obj = object.asObject();
        return obj.find(propertyName) != obj.end();
    }

    FFIValue JavaScriptFFIImpl::createArray(const std::vector<FFIValue>& elements) {
        return FFIValue(elements);
    }

    FFIValue JavaScriptFFIImpl::getArrayElement(const FFIValue& array, size_t index) {
        if (!array.isArray()) {
            return FFIValue();
        }
        const auto& arr = array.asArray();
        if (index < arr.size()) {
            return arr[index];
        }
        return FFIValue();
    }

    bool JavaScriptFFIImpl::setArrayElement(FFIValue& array, size_t index, const FFIValue& value) {
        if (!array.isArray()) {
            return false;
        }
        auto& arr = array.asArray();
        if (index < arr.size()) {
            arr[index] = value;
            return true;
        }
        return false;
    }

    size_t JavaScriptFFIImpl::getArrayLength(const FFIValue& array) {
        if (!array.isArray()) {
            return 0;
        }
        return array.asArray().size();
    }

    bool JavaScriptFFIImpl::pushToArray(FFIValue& array, const FFIValue& value) {
        if (!array.isArray()) {
            return false;
        }
        array.asArray().push_back(value);
        return true;
    }

    FFIValue JavaScriptFFIImpl::getGlobal(const std::string& name) {
        return getVariable(name);
    }
    
    bool JavaScriptFFIImpl::setGlobal(const std::string& name, const FFIValue& value) {
        setVariable(name, value);
        return true;
    }

    JavaScriptFFIImpl::Promise JavaScriptFFIImpl::createPromise() {
        return Promise{Promise::PENDING, FFIValue(), ""};
    }

    bool JavaScriptFFIImpl::resolvePromise(Promise& promise, const FFIValue& value) {
        if (promise.state != Promise::PENDING) {
            return false;
        }
        promise.state = Promise::FULFILLED;
        promise.value = value;
        return true;
    }

    bool JavaScriptFFIImpl::rejectPromise(Promise& promise, const std::string& reason) {
        if (promise.state != Promise::PENDING) {
            return false;
        }
        promise.state = Promise::REJECTED;
        promise.reason = reason;
        return true;
    }

    FFIValue JavaScriptFFIImpl::promiseToFFIValue(const Promise& promise) {
        // Convert promise to an object representation
        std::unordered_map<std::string, FFIValue> promiseObj;
        promiseObj["state"] = FFIValue(static_cast<int>(promise.state));
        promiseObj["value"] = promise.value;
        promiseObj["reason"] = FFIValue(promise.reason);
        return FFIValue(promiseObj);
    }

    FFIValue JavaScriptFFIImpl::awaitPromise(const FFIValue& promise) {
        // Stub: would need async/await infrastructure
        if (!promise.isObject()) {
            return FFIValue();
        }
        auto obj = promise.asObject();
        auto stateIt = obj.find("state");
        auto valueIt = obj.find("value");
        
        if (stateIt != obj.end() && valueIt != obj.end()) {
            int state = stateIt->second.asInt32();
            if (state == Promise::FULFILLED) {
                return valueIt->second;
            }
        }
        return FFIValue();
    }

    bool JavaScriptFFIImpl::isPromise(const FFIValue& value) {
        if (!value.isObject()) {
            return false;
        }
        const auto& obj = value.asObject();
        return obj.find("state") != obj.end() && 
               obj.find("value") != obj.end() && 
               obj.find("reason") != obj.end();
    }

    bool JavaScriptFFIImpl::isJavaScriptObject(const FFIValue& value) const {
        return value.isObject();
    }

    std::string JavaScriptFFIImpl::getJavaScriptTypeName(const FFIValue& value) const {
        switch (value.getType()) {
            case FFIValue::UNDEFINED: return "undefined";
            case FFIValue::NULL_VALUE: return "null";
            case FFIValue::BOOLEAN: return "boolean";
            case FFIValue::INTEGER:
            case FFIValue::FLOAT: return "number";
            case FFIValue::STRING: return "string";
            case FFIValue::ARRAY: return "array";
            case FFIValue::OBJECT: return "object";
            case FFIValue::FUNCTION: return "function";
            default: return "unknown";
        }
    }

    // Conversion helpers
    FFIValue JavaScriptFFIImpl::ffiValueToJS(const FFIValue& value) {
        // Direct pass-through for now
        return value;
    }

    FFIValue JavaScriptFFIImpl::jsValueToFFI(const FFIValue& value) {
        // Direct pass-through for now
        return value;
    }

} // namespace ffi
