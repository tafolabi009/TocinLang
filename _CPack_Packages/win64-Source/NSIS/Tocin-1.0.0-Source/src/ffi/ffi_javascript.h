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
 * @brief JavaScript FFI implementation that conforms to FFIInterface
 */
class JavaScriptFFIImpl : public FFIInterface {
public:
    JavaScriptFFIImpl();
    ~JavaScriptFFIImpl() override;

    // FFIInterface implementation
    bool initialize() override;
    void finalize() override;
    bool isInitialized() const override { return initialized_; }

    std::string getLanguageName() const override { return "JavaScript"; }
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

    // JavaScript-specific methods
    FFIValue executeCode(const std::string& code);
    FFIValue executeFile(const std::string& filename);
    bool loadModuleFromCode(const std::string& moduleName, const std::string& code);

    FFIValue callMethod(const FFIValue& object, const std::string& methodName, const std::vector<FFIValue>& args);
    
    // Object operations
    FFIValue createObject(const std::unordered_map<std::string, FFIValue>& properties);
    FFIValue getProperty(const FFIValue& object, const std::string& propertyName);
    bool setProperty(FFIValue& object, const std::string& propertyName, const FFIValue& value);
    bool hasProperty(const FFIValue& object, const std::string& propertyName);

    // Array operations
    FFIValue createArray(const std::vector<FFIValue>& elements);
    FFIValue getArrayElement(const FFIValue& array, size_t index);
    bool setArrayElement(FFIValue& array, size_t index, const FFIValue& value);
    size_t getArrayLength(const FFIValue& array);
    bool pushToArray(FFIValue& array, const FFIValue& value);

    // Global object access
    FFIValue getGlobal(const std::string& name);
    bool setGlobal(const std::string& name, const FFIValue& value);

    // Promise support
    struct Promise {
        enum State { PENDING, FULFILLED, REJECTED };
        State state;
        FFIValue value;
        std::string reason;
    };

    Promise createPromise();
    bool resolvePromise(Promise& promise, const FFIValue& value);
    bool rejectPromise(Promise& promise, const std::string& reason);
    FFIValue promiseToFFIValue(const Promise& promise);

    // Async/await support
    FFIValue awaitPromise(const FFIValue& promise);
    bool isPromise(const FFIValue& value);

    // JavaScript-specific type checking
    bool isJavaScriptObject(const FFIValue& value) const;
    std::string getJavaScriptTypeName(const FFIValue& value) const;

private:
    bool initialized_;
    std::unique_ptr<JavaScriptFFI> jsFFI_;
    
    // Conversion helpers
    FFIValue ffiValueToJS(const FFIValue& value);
    FFIValue jsValueToFFI(const FFIValue& value);
};

/**
 * @brief JavaScript module wrapper for easier module management
 */
class JavaScriptModule {
public:
    JavaScriptModule(const std::string& name, JavaScriptFFIImpl* ffi);
    ~JavaScriptModule();

    bool load();
    bool loadFromCode(const std::string& code);
    bool unload();
    bool isLoaded() const { return loaded_; }

    FFIValue callFunction(const std::string& functionName, const std::vector<FFIValue>& args);
    bool hasFunction(const std::string& functionName) const;

    FFIValue getExport(const std::string& exportName);
    bool setExport(const std::string& exportName, const FFIValue& value);

    std::vector<std::string> getExportNames() const;
    const std::string& getName() const { return name_; }

private:
    std::string name_;
    JavaScriptFFIImpl* ffi_;
    bool loaded_;
    FFIValue moduleObject_;
};

/**
 * @brief JavaScript class wrapper for easier class instantiation and method calling
 */
class JavaScriptClass {
public:
    JavaScriptClass(const std::string& className, const std::string& moduleName, JavaScriptFFIImpl* ffi);
    ~JavaScriptClass();

    FFIValue createInstance(const std::vector<FFIValue>& args = {});
    FFIValue callStaticMethod(const std::string& methodName, const std::vector<FFIValue>& args);
    
    bool hasMethod(const std::string& methodName) const;
    bool hasStaticMethod(const std::string& methodName) const;

    const std::string& getClassName() const { return className_; }
    const std::string& getModuleName() const { return moduleName_; }

private:
    std::string className_;
    std::string moduleName_;
    JavaScriptFFIImpl* ffi_;
    FFIValue classObject_;
};

/**
 * @brief JavaScript instance wrapper for easier method calling and property access
 */
class JavaScriptInstance {
public:
    JavaScriptInstance(const FFIValue& instance, JavaScriptFFIImpl* ffi);
    ~JavaScriptInstance();

    FFIValue callMethod(const std::string& methodName, const std::vector<FFIValue>& args);
    
    FFIValue getProperty(const std::string& propertyName);
    bool setProperty(const std::string& propertyName, const FFIValue& value);
    
    bool hasMethod(const std::string& methodName) const;
    bool hasProperty(const std::string& propertyName) const;

    const FFIValue& getInstance() const { return instance_; }
    std::string getTypeName() const;

private:
    FFIValue instance_;
    JavaScriptFFIImpl* ffi_;
};

/**
 * @brief Node.js integration wrapper
 */
class NodeJSIntegration {
public:
    NodeJSIntegration(JavaScriptFFIImpl* ffi);
    ~NodeJSIntegration();

    bool initialize();
    void finalize();

    // Module system
    bool requireModule(const std::string& moduleName);
    FFIValue callNodeFunction(const std::string& moduleName, const std::string& functionName,
                             const std::vector<FFIValue>& args);

    // File system operations
    FFIValue readFile(const std::string& filename);
    bool writeFile(const std::string& filename, const std::string& content);
    bool fileExists(const std::string& filename);

    // HTTP operations
    struct HTTPRequest {
        std::string method;
        std::string url;
        std::unordered_map<std::string, std::string> headers;
        std::string body;
    };

    struct HTTPResponse {
        int statusCode;
        std::unordered_map<std::string, std::string> headers;
        std::string body;
    };

    HTTPResponse makeHTTPRequest(const HTTPRequest& request);

    // Process operations
    FFIValue getProcessEnv();
    std::string getProcessCwd();
    std::vector<std::string> getProcessArgv();

private:
    JavaScriptFFIImpl* ffi_;
    std::unique_ptr<NodeJSIntegration> nodeIntegration_;
};

/**
 * @brief Browser JavaScript integration wrapper
 */
class BrowserJSIntegration {
public:
    BrowserJSIntegration(JavaScriptFFIImpl* ffi);
    ~BrowserJSIntegration();

    bool initialize();
    void finalize();

    // DOM operations
    struct DOMElement {
        std::string tagName;
        std::string id;
        std::string className;
        std::unordered_map<std::string, std::string> attributes;
        std::string textContent;
        std::vector<std::shared_ptr<DOMElement>> children;
    };

    std::shared_ptr<DOMElement> createElement(const std::string& tagName);
    std::shared_ptr<DOMElement> getElementById(const std::string& id);
    std::vector<std::shared_ptr<DOMElement>> getElementsByClassName(const std::string& className);
    std::vector<std::shared_ptr<DOMElement>> getElementsByTagName(const std::string& tagName);

    bool appendChild(std::shared_ptr<DOMElement> parent, std::shared_ptr<DOMElement> child);
    bool removeChild(std::shared_ptr<DOMElement> parent, std::shared_ptr<DOMElement> child);

    // Event handling
    using EventHandler = std::function<void(const FFIValue&)>;
    bool addEventListener(std::shared_ptr<DOMElement> element, const std::string& eventType, EventHandler handler);
    bool removeEventListener(std::shared_ptr<DOMElement> element, const std::string& eventType);

    // Window operations
    FFIValue getWindow();
    FFIValue getDocument();
    FFIValue getLocation();

    // Storage operations
    bool setLocalStorage(const std::string& key, const std::string& value);
    std::string getLocalStorage(const std::string& key);
    bool removeLocalStorage(const std::string& key);

    bool setSessionStorage(const std::string& key, const std::string& value);
    std::string getSessionStorage(const std::string& key);
    bool removeSessionStorage(const std::string& key);

    // AJAX operations
    struct AjaxRequest {
        std::string method;
        std::string url;
        std::unordered_map<std::string, std::string> headers;
        std::string data;
        bool async;
    };

    struct AjaxResponse {
        int status;
        std::string statusText;
        std::string responseText;
        FFIValue responseJSON;
    };

    AjaxResponse makeAjaxRequest(const AjaxRequest& request);

private:
    JavaScriptFFIImpl* ffi_;
    std::unique_ptr<BrowserJSIntegration> browserIntegration_;
    std::shared_ptr<DOMElement> documentRoot_;
    std::unordered_map<std::string, std::vector<EventHandler>> eventHandlers_;
};

/**
 * @brief JavaScript integration utilities
 */
class JavaScriptUtils {
public:
    // String utilities
    static std::string escapeJavaScriptString(const std::string& str);
    static std::string unescapeJavaScriptString(const std::string& str);
    
    // Code generation utilities
    static std::string generateJavaScriptWrapper(const std::string& functionName, 
                                                const std::vector<std::string>& paramTypes,
                                                const std::string& returnType);
    static std::string generateClassWrapper(const std::string& className,
                                          const std::vector<std::string>& methods);

    // Type conversion utilities
    static std::string tocinTypeToJavaScript(const std::string& tocinType);
    static std::string javaScriptTypeToTocin(const std::string& jsType);
    
    // Error handling utilities
    static bool isJavaScriptError(const FFIValue& value);
    static std::string extractJavaScriptError(const FFIValue& value);
    static FFIValue createJavaScriptError(const std::string& errorType, const std::string& message);

    // JSON utilities
    static FFIValue parseJSON(const std::string& json);
    static std::string stringifyJSON(const FFIValue& value);
    static bool isValidJSON(const std::string& json);

    // Performance utilities
    static void enableJavaScriptProfiling(bool enable);
    static std::string getJavaScriptProfilingResults();

    // Promise utilities
    static bool isPromiseLike(const FFIValue& value);
    static FFIValue createResolvedPromise(const FFIValue& value);
    static FFIValue createRejectedPromise(const std::string& reason);

private:
    JavaScriptUtils() = delete;
};

} // namespace ffi
