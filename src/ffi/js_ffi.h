#pragma once

#include "../ast/types.h"
#include "../ast/ast.h"
#include "../error/error_handler.h"
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>

namespace ffi {

/**
 * @brief JavaScript FFI interface using V8 engine
 */
class JavaScriptFFI {
public:
    JavaScriptFFI();
    ~JavaScriptFFI();

    // Initialize V8 engine
    bool initialize();
    void finalize();

    // JavaScript value representation
    struct JSValue {
        enum Type { UNDEFINED, NULL_VAL, BOOLEAN, NUMBER, STRING, OBJECT, ARRAY, FUNCTION };
        Type type;
        union {
            bool boolValue;
            double numberValue;
        };
        std::string stringValue;
        std::unordered_map<std::string, JSValue> objectValue;
        std::vector<JSValue> arrayValue;
        std::function<JSValue(const std::vector<JSValue>&)> functionValue;

        JSValue() : type(UNDEFINED) {}
        JSValue(bool value) : type(BOOLEAN), boolValue(value) {}
        JSValue(double value) : type(NUMBER), numberValue(value) {}
        JSValue(const std::string& value) : type(STRING), stringValue(value) {}
        JSValue(const std::unordered_map<std::string, JSValue>& value) : type(OBJECT), objectValue(value) {}
        JSValue(const std::vector<JSValue>& value) : type(ARRAY), arrayValue(value) {}
        
        static JSValue undefined() { return JSValue(); }
        static JSValue null() { JSValue val; val.type = NULL_VAL; return val; }
    };

    // Code execution
    JSValue executeCode(const std::string& code);
    JSValue executeFile(const std::string& filename);
    bool loadModule(const std::string& moduleName, const std::string& code);

    // Function calling
    JSValue callFunction(const std::string& functionName, const std::vector<JSValue>& args);
    JSValue callMethod(const JSValue& object, const std::string& methodName, const std::vector<JSValue>& args);

    // Object operations
    JSValue createObject(const std::unordered_map<std::string, JSValue>& properties);
    JSValue getProperty(const JSValue& object, const std::string& propertyName);
    bool setProperty(JSValue& object, const std::string& propertyName, const JSValue& value);
    bool hasProperty(const JSValue& object, const std::string& propertyName);

    // Array operations
    JSValue createArray(const std::vector<JSValue>& elements);
    JSValue getArrayElement(const JSValue& array, size_t index);
    bool setArrayElement(JSValue& array, size_t index, const JSValue& value);
    size_t getArrayLength(const JSValue& array);
    bool pushToArray(JSValue& array, const JSValue& value);

    // Type conversion
    JSValue toJSValue(ast::ValuePtr value);
    ast::ValuePtr fromJSValue(const JSValue& value);

    // Global object access
    JSValue getGlobal(const std::string& name);
    bool setGlobal(const std::string& name, const JSValue& value);

    // Error handling
    bool hasError() const { return hasError_; }
    std::string getLastError() const { return lastError_; }

    // Promise support
    struct Promise {
        enum State { PENDING, FULFILLED, REJECTED };
        State state;
        JSValue value;
        std::string reason;
        
        Promise() : state(PENDING) {}
    };

    Promise createPromise();
    bool resolvePromise(Promise& promise, const JSValue& value);
    bool rejectPromise(Promise& promise, const std::string& reason);
    JSValue promiseToJSValue(const Promise& promise);

    // Async/await support
    JSValue awaitPromise(const JSValue& promise);
    bool isPromise(const JSValue& value);

private:
    bool initialized_;
    bool hasError_;
    std::string lastError_;
    void* isolate_;  // v8::Isolate* in actual implementation
    void* context_;  // v8::Local<v8::Context> in actual implementation
    std::unordered_map<std::string, void*> modules_;

    void setError(const std::string& error);
    void clearError();
    void* toV8Value(const JSValue& value);
    JSValue fromV8Value(void* v8Value);
};

/**
 * @brief Node.js integration for server-side JavaScript
 */
class NodeJSIntegration {
public:
    NodeJSIntegration();
    ~NodeJSIntegration();

    bool initialize();
    void finalize();

    // Module system
    bool requireModule(const std::string& moduleName);
    JavaScriptFFI::JSValue callNodeFunction(const std::string& moduleName, 
                                           const std::string& functionName,
                                           const std::vector<JavaScriptFFI::JSValue>& args);

    // File system operations
    JavaScriptFFI::JSValue readFile(const std::string& filename);
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
    JavaScriptFFI::JSValue getProcessEnv();
    std::string getProcessCwd();
    std::vector<std::string> getProcessArgv();

private:
    JavaScriptFFI jsEngine_;
    std::unordered_map<std::string, void*> nodeModules_;
};

/**
 * @brief Browser JavaScript integration
 */
class BrowserJSIntegration {
public:
    BrowserJSIntegration();
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
    using EventHandler = std::function<void(const JavaScriptFFI::JSValue&)>;
    bool addEventListener(std::shared_ptr<DOMElement> element, const std::string& eventType, EventHandler handler);
    bool removeEventListener(std::shared_ptr<DOMElement> element, const std::string& eventType);

    // Window operations
    JavaScriptFFI::JSValue getWindow();
    JavaScriptFFI::JSValue getDocument();
    JavaScriptFFI::JSValue getLocation();

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
        JavaScriptFFI::JSValue responseJSON;
    };

    AjaxResponse makeAjaxRequest(const AjaxRequest& request);

private:
    JavaScriptFFI jsEngine_;
    std::shared_ptr<DOMElement> documentRoot_;
    std::unordered_map<std::string, std::vector<EventHandler>> eventHandlers_;
};

/**
 * @brief React/JSX integration
 */
class ReactIntegration {
public:
    ReactIntegration();
    ~ReactIntegration();

    bool initialize();
    void finalize();

    // Component definition
    struct ReactComponent {
        std::string name;
        std::function<JavaScriptFFI::JSValue(const JavaScriptFFI::JSValue&)> render;
        std::unordered_map<std::string, JavaScriptFFI::JSValue> defaultProps;
        std::unordered_map<std::string, JavaScriptFFI::JSValue> initialState;
    };

    bool defineComponent(const ReactComponent& component);
    JavaScriptFFI::JSValue createComponent(const std::string& componentName, 
                                          const JavaScriptFFI::JSValue& props);

    // JSX transformation
    std::string transformJSX(const std::string& jsxCode);
    JavaScriptFFI::JSValue executeJSX(const std::string& jsxCode);

    // Virtual DOM operations
    JavaScriptFFI::JSValue createElement(const std::string& type, 
                                        const JavaScriptFFI::JSValue& props,
                                        const std::vector<JavaScriptFFI::JSValue>& children);
    
    std::string renderToString(const JavaScriptFFI::JSValue& element);
    bool renderToDOM(const JavaScriptFFI::JSValue& element, const std::string& containerId);

private:
    JavaScriptFFI jsEngine_;
    std::unordered_map<std::string, ReactComponent> components_;
};

} // namespace ffi