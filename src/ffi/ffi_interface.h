#ifndef TOCIN_FFI_INTERFACE_H
#define TOCIN_FFI_INTERFACE_H

#include <string>
#include <vector>
#include <memory>
#include <variant>
#include <unordered_map>
#include <functional>
#include <any>

namespace ffi {

// Forward declarations
class FFIValue;
class PythonFFI;
class JavaScriptFFI;
class CppFFI; // defined in cpp_ffi.h

/**
 * @brief FFI value types supported by Tocin
 */
enum class FFIType {
    NIL,
    BOOL,
    INT,
    FLOAT,
    STRING,
    ARRAY,
    OBJECT,
    FUNCTION
};

// Note: FFIValue is defined in ffi_value.h. We forward-declare it here.

/**
 * @brief Base FFI interface
 */
class FFIInterface {
public:
    virtual ~FFIInterface() = default;
    
    /**
     * @brief Initialize the FFI system
     */
    virtual bool initialize() = 0;
    
    /**
     * @brief Cleanup the FFI system
     */
    virtual void cleanup() = 0;
    
    /**
     * @brief Call a function in the foreign language
     */
    virtual FFIValue call(const std::string& function, const std::vector<FFIValue>& args) = 0;
    
    /**
     * @brief Evaluate code in the foreign language
     */
    virtual FFIValue eval(const std::string& code) = 0;
    
    /**
     * @brief Get a variable from the foreign language
     */
    virtual FFIValue getVariable(const std::string& name) = 0;
    
    /**
     * @brief Set a variable in the foreign language
     */
    virtual void setVariable(const std::string& name, const FFIValue& value) = 0;
    
    /**
     * @brief Check if the FFI system is available
     */
    virtual bool isAvailable() const = 0;
};

/**
 * @brief Python FFI implementation
 */
class PythonFFI : public FFIInterface {
private:
    void* python_state_;
    std::unordered_map<std::string, std::any> variables_;
    bool initialized_;

public:
    PythonFFI();
    ~PythonFFI() override;

    bool initialize() override;
    void cleanup() override;
    FFIValue call(const std::string& function, const std::vector<FFIValue>& args) override;
    FFIValue eval(const std::string& code) override;
    FFIValue getVariable(const std::string& name) override;
    void setVariable(const std::string& name, const FFIValue& value) override;
    bool isAvailable() const override;

private:
    FFIValue convertPythonObject(void* obj);
    void* convertToPythonObject(const FFIValue& value);
    std::string getPythonError();
};

/**
 * @brief JavaScript FFI implementation
 */
class JavaScriptFFI : public FFIInterface {
private:
    void* v8_isolate_;
    void* v8_context_;
    std::unordered_map<std::string, std::any> variables_;
    bool initialized_;

public:
    JavaScriptFFI();
    ~JavaScriptFFI() override;

    bool initialize() override;
    void cleanup() override;
    FFIValue call(const std::string& function, const std::vector<FFIValue>& args) override;
    FFIValue eval(const std::string& code) override;
    FFIValue getVariable(const std::string& name) override;
    void setVariable(const std::string& name, const FFIValue& value) override;
    bool isAvailable() const override;

private:
    FFIValue convertJSValue(void* value);
    void* convertToJSValue(const FFIValue& value);
    std::string getJSError();
};

/**
 * @brief C++ FFI implementation
 */
class CppFFI : public FFIInterface {
private:
    std::unordered_map<std::string, std::function<FFIValue(const std::vector<FFIValue>&)>> functions_;
    std::unordered_map<std::string, FFIValue> variables_;
    bool initialized_;

public:
    CppFFI();
    ~CppFFI() override;

    bool initialize() override;
    void cleanup() override;
    FFIValue call(const std::string& function, const std::vector<FFIValue>& args) override;
    FFIValue eval(const std::string& code) override;
    FFIValue getVariable(const std::string& name) override;
    void setVariable(const std::string& name, const FFIValue& value) override;
    bool isAvailable() const override;

    /**
     * @brief Register a C++ function
     */
    template<typename F>
    void registerFunction(const std::string& name, F&& func) {
        functions_[name] = std::forward<F>(func);
    }

    /**
     * @brief Register a C++ variable
     */
    void registerVariable(const std::string& name, const FFIValue& value) {
        variables_.insert_or_assign(name, value);
    }
};

/**
 * @brief Main FFI manager
 */
class FFIManager {
private:
    std::unique_ptr<PythonFFI> python_ffi_;
    std::unique_ptr<JavaScriptFFI> js_ffi_;
    std::unique_ptr<CppFFI> cpp_ffi_;
    std::unordered_map<std::string, std::shared_ptr<FFIInterface>> interfaces_;

public:
    FFIManager();
    ~FFIManager();

    /**
     * @brief Initialize all FFI systems
     */
    bool initialize();

    /**
     * @brief Cleanup all FFI systems
     */
    void cleanup();

    /**
     * @brief Get Python FFI
     */
    PythonFFI* getPythonFFI() { return python_ffi_.get(); }

    /**
     * @brief Get JavaScript FFI
     */
    JavaScriptFFI* getJavaScriptFFI() { return js_ffi_.get(); }

    /**
     * @brief Get C++ FFI
     */
    CppFFI* getCppFFI() { return cpp_ffi_.get(); }

    /**
     * @brief Call a function in a specific language
     */
    FFIValue call(const std::string& language, const std::string& function, 
                  const std::vector<FFIValue>& args);

    /**
     * @brief Evaluate code in a specific language
     */
    FFIValue eval(const std::string& language, const std::string& code);

    /**
     * @brief Get a variable from a specific language
     */
    FFIValue getVariable(const std::string& language, const std::string& name);

    /**
     * @brief Set a variable in a specific language
     */
    void setVariable(const std::string& language, const std::string& name, const FFIValue& value);

    /**
     * @brief Check if a language is available
     */
    bool isLanguageAvailable(const std::string& language) const;

    /**
     * @brief Get available languages
     */
    std::vector<std::string> getAvailableLanguages() const;
};

// Global FFI manager instance
extern std::unique_ptr<FFIManager> global_ffi_manager;

/**
 * @brief Initialize the global FFI manager
 */
bool initializeFFI();

/**
 * @brief Cleanup the global FFI manager
 */
void cleanupFFI();

/**
 * @brief Get the global FFI manager
 */
FFIManager& getFFIManager();

} // namespace ffi

#endif // TOCIN_FFI_INTERFACE_H