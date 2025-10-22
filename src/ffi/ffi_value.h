#pragma once

#include "../ast/types.h"
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <variant>
#include <cstdint>
#include <functional>

namespace ffi {

/**
 * @brief Universal value type for FFI operations
 */
class FFIValue {
public:
    enum Type {
        UNDEFINED,
        NULL_VALUE,
        BOOLEAN,
        INTEGER,
        FLOAT,
        STRING,
        ARRAY,
        OBJECT,
        FUNCTION,
        POINTER,
        BINARY_DATA
    };

    // Constructors
    FFIValue();
    FFIValue(bool value);
    FFIValue(int32_t value);
    FFIValue(int64_t value);
    FFIValue(float value);
    FFIValue(double value);
    FFIValue(const std::string& value);
    FFIValue(const char* value);
    FFIValue(const std::vector<FFIValue>& value);
    FFIValue(const std::unordered_map<std::string, FFIValue>& value);
    FFIValue(void* pointer, const std::string& typeName = "");

    // Copy and move constructors
    FFIValue(const FFIValue& other);
    FFIValue(FFIValue&& other) noexcept;

    // Assignment operators
    FFIValue& operator=(const FFIValue& other);
    FFIValue& operator=(FFIValue&& other) noexcept;

    // Destructor
    ~FFIValue();

    // Type checking
    Type getType() const { return type_; }
    bool isUndefined() const { return type_ == UNDEFINED; }
    bool isNull() const { return type_ == NULL_VALUE; }
    bool isBoolean() const { return type_ == BOOLEAN; }
    bool isInteger() const { return type_ == INTEGER; }
    bool isFloat() const { return type_ == FLOAT; }
    bool isNumber() const { return type_ == INTEGER || type_ == FLOAT; }
    bool isString() const { return type_ == STRING; }
    bool isArray() const { return type_ == ARRAY; }
    bool isObject() const { return type_ == OBJECT; }
    bool isFunction() const { return type_ == FUNCTION; }
    bool isPointer() const { return type_ == POINTER; }
    bool isBinaryData() const { return type_ == BINARY_DATA; }

    // Value extraction
    bool asBoolean() const;
    int32_t asInt32() const;
    int64_t asInt64() const;
    float asFloat() const;
    double asDouble() const;
    std::string asString() const;
    const std::vector<FFIValue>& asArray() const;
    std::vector<FFIValue>& asArray();
    const std::unordered_map<std::string, FFIValue>& asObject() const;
    std::unordered_map<std::string, FFIValue>& asObject();
    void* asPointer() const;
    const std::vector<uint8_t>& asBinaryData() const;

    // Array operations
    size_t arraySize() const;
    FFIValue arrayGet(size_t index) const;
    void arraySet(size_t index, const FFIValue& value);
    void arrayPush(const FFIValue& value);
    FFIValue arrayPop();
    void arrayClear();

    // Object operations
    size_t objectSize() const;
    bool objectHas(const std::string& key) const;
    FFIValue objectGet(const std::string& key) const;
    void objectSet(const std::string& key, const FFIValue& value);
    void objectRemove(const std::string& key);
    std::vector<std::string> objectKeys() const;
    void objectClear();

    // Type conversion
    FFIValue convertTo(Type targetType) const;
    bool canConvertTo(Type targetType) const;

    // Serialization
    std::string toJSON() const;
    static FFIValue fromJSON(const std::string& json);
    
    std::vector<uint8_t> toBinary() const;
    static FFIValue fromBinary(const std::vector<uint8_t>& data);

    // String representation
    std::string toString() const;
    std::string getTypeName() const;

    // Comparison operators
    bool operator==(const FFIValue& other) const;
    bool operator!=(const FFIValue& other) const;
    bool operator<(const FFIValue& other) const;
    bool operator<=(const FFIValue& other) const;
    bool operator>(const FFIValue& other) const;
    bool operator>=(const FFIValue& other) const;

    // Arithmetic operators (for numeric types)
    FFIValue operator+(const FFIValue& other) const;
    FFIValue operator-(const FFIValue& other) const;
    FFIValue operator*(const FFIValue& other) const;
    FFIValue operator/(const FFIValue& other) const;
    FFIValue operator%(const FFIValue& other) const;

    // Logical operators
    FFIValue operator&&(const FFIValue& other) const;
    FFIValue operator||(const FFIValue& other) const;
    FFIValue operator!() const;

    // Utility methods
    bool isEmpty() const;
    size_t getSize() const;
    FFIValue clone() const;
    void reset();

    // Memory management for pointers
    void setPointerOwnership(bool owned) { pointerOwned_ = owned; }
    bool hasPointerOwnership() const { return pointerOwned_; }
    void setPointerTypeName(const std::string& typeName) { pointerTypeName_ = typeName; }
    const std::string& getPointerTypeName() const { return pointerTypeName_; }
    
    // Metadata support for FFI operations
    void setMetadata(const std::string& key, const std::string& value) { 
        metadata_[key] = value; 
    }
    std::string getMetadata(const std::string& key) const {
        auto it = metadata_.find(key);
        return it != metadata_.end() ? it->second : "";
    }
    bool hasMetadata(const std::string& key) const {
        return metadata_.find(key) != metadata_.end();
    }
    void clearMetadata() { metadata_.clear(); }

    // Function call support (for function values)
    using FunctionCallback = std::function<FFIValue(const std::vector<FFIValue>&)>;
    void setFunction(FunctionCallback callback);
    FFIValue callFunction(const std::vector<FFIValue>& args) const;

    // Error handling
    static FFIValue createError(const std::string& message);
    bool isError() const;
    std::string getErrorMessage() const;

    // Factory methods
    static FFIValue createUndefined();
    static FFIValue createNull();
    static FFIValue createArray(size_t size = 0);
    static FFIValue createObject();
    static FFIValue createBinaryData(const void* data, size_t size);

private:
    Type type_;
    
    // Value storage using variant
    std::variant<
        std::monostate,     // UNDEFINED, NULL_VALUE
        bool,               // BOOLEAN
        int64_t,            // INTEGER
        double,             // FLOAT
        std::string,        // STRING
        std::vector<FFIValue>,  // ARRAY
        std::unordered_map<std::string, FFIValue>,  // OBJECT
        void*,              // POINTER
        std::vector<uint8_t>    // BINARY_DATA
    > value_;

    // Additional metadata
    std::string pointerTypeName_;
    bool pointerOwned_;
    FunctionCallback functionCallback_;
    std::string errorMessage_;
    std::unordered_map<std::string, std::string> metadata_;

    // Helper methods
    void cleanup();
    void copyFrom(const FFIValue& other);
    void moveFrom(FFIValue&& other);
    
    // Conversion helpers
    template<typename T>
    T getVariantValue() const;
    
    template<typename T>
    void setVariantValue(const T& value);
};

/**
 * @brief FFI value utilities
 */
class FFIValueUtils {
public:
    // Type checking utilities
    static bool areTypesCompatible(FFIValue::Type type1, FFIValue::Type type2);
    static FFIValue::Type getCommonType(FFIValue::Type type1, FFIValue::Type type2);
    static std::string typeToString(FFIValue::Type type);
    static FFIValue::Type stringToType(const std::string& typeStr);

    // Conversion utilities
    static FFIValue convertNumber(const FFIValue& value, FFIValue::Type targetType);
    static FFIValue convertToString(const FFIValue& value);
    static FFIValue convertToBoolean(const FFIValue& value);

    // Array utilities
    static FFIValue createArrayFromVector(const std::vector<int>& vec);
    static FFIValue createArrayFromVector(const std::vector<double>& vec);
    static FFIValue createArrayFromVector(const std::vector<std::string>& vec);
    static std::vector<int> arrayToIntVector(const FFIValue& array);
    static std::vector<double> arrayToDoubleVector(const FFIValue& array);
    static std::vector<std::string> arrayToStringVector(const FFIValue& array);

    // Object utilities
    static FFIValue createObjectFromMap(const std::unordered_map<std::string, int>& map);
    static FFIValue createObjectFromMap(const std::unordered_map<std::string, double>& map);
    static FFIValue createObjectFromMap(const std::unordered_map<std::string, std::string>& map);

    // Validation utilities
    static bool isValidJSON(const std::string& json);
    static bool isValidBinary(const std::vector<uint8_t>& data);
    static bool validateValue(const FFIValue& value);

    // Deep operations
    static FFIValue deepClone(const FFIValue& value);
    static bool deepEquals(const FFIValue& value1, const FFIValue& value2);
    static void deepMerge(FFIValue& target, const FFIValue& source);

    // Memory utilities
    static size_t calculateMemoryUsage(const FFIValue& value);
    static void optimizeMemoryUsage(FFIValue& value);

private:
    FFIValueUtils() = delete;
};

} // namespace ffi