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
class CppFFI;

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

/**
 * @brief FFI value representation
 */
class FFIValue {
private:
    FFIType type_;
    std::variant<std::monostate, bool, int64_t, double, std::string, 
                 std::vector<FFIValue>, std::unordered_map<std::string, FFIValue>> value_;

public:
    FFIValue() : type_(FFIType::NIL) {}
    FFIValue(bool value) : type_(FFIType::BOOL), value_(value) {}
    FFIValue(int64_t value) : type_(FFIType::INT), value_(value) {}
    FFIValue(double value) : type_(FFIType::FLOAT), value_(value) {}
    FFIValue(const std::string& value) : type_(FFIType::STRING), value_(value) {}
    FFIValue(const std::vector<FFIValue>& value) : type_(FFIType::ARRAY), value_(value) {}
    FFIValue(const std::unordered_map<std::string, FFIValue>& value) : type_(FFIType::OBJECT), value_(value) {}

    FFIType getType() const { return type_; }

    bool asBool() const {
        if (type_ == FFIType::BOOL) return std::get<bool>(value_);
        if (type_ == FFIType::INT) return std::get<int64_t>(value_) != 0;
        if (type_ == FFIType::FLOAT) return std::get<double>(value_) != 0.0;
        return false;
    }

    int64_t asInt() const {
        if (type_ == FFIType::INT) return std::get<int64_t>(value_);
        if (type_ == FFIType::FLOAT) return static_cast<int64_t>(std::get<double>(value_));
        if (type_ == FFIType::BOOL) return std::get<bool>(value_) ? 1 : 0;
        return 0;
    }

    double asFloat() const {
        if (type_ == FFIType::FLOAT) return std::get<double>(value_);
        if (type_ == FFIType::INT) return static_cast<double>(std::get<int64_t>(value_));
        if (type_ == FFIType::BOOL) return std::get<bool>(value_) ? 1.0 : 0.0;
        return 0.0;
    }

    std::string asString() const {
        if (type_ == FFIType::STRING) return std::get<std::string>(value_);
        if (type_ == FFIType::INT) return std::to_string(std::get<int64_t>(value_));
        if (type_ == FFIType::FLOAT) return std::to_string(std::get<double>(value_));
        if (type_ == FFIType::BOOL) return std::get<bool>(value_) ? "true" : "false";
        return "";
    }

    std::vector<FFIValue> asArray() const {
        if (type_ == FFIType::ARRAY) return std::get<std::vector<FFIValue>>(value_);
        return {};
    }

    std::unordered_map<std::string, FFIValue> asObject() const {
        if (type_ == FFIType::OBJECT) return std::get<std::unordered_map<std::string, FFIValue>>(value_);
        return {};
    }

    bool isNil() const { return type_ == FFIType::NIL; }
    bool isBool() const { return type_ == FFIType::BOOL; }
    bool isInt() const { return type_ == FFIType::INT; }
    bool isFloat() const { return type_ == FFIType::FLOAT; }
    bool isString() const { return type_ == FFIType::STRING; }
    bool isArray() const { return type_ == FFIType::ARRAY; }
    bool isObject() const { return type_ == FFIType::OBJECT; }
};

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
        variables_[name] = value;
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