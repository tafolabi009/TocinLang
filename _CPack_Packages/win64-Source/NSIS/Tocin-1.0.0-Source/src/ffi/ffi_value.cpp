#include "ffi_value.h"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cstring>

namespace ffi {

// FFIValue implementation
FFIValue::FFIValue() : type_(UNDEFINED), pointerOwned_(false) {}

FFIValue::FFIValue(bool value) : type_(BOOLEAN), pointerOwned_(false) {
    setVariantValue(value);
}

FFIValue::FFIValue(int32_t value) : type_(INTEGER), pointerOwned_(false) {
    setVariantValue(static_cast<int64_t>(value));
}

FFIValue::FFIValue(int64_t value) : type_(INTEGER), pointerOwned_(false) {
    setVariantValue(value);
}

FFIValue::FFIValue(float value) : type_(FLOAT), pointerOwned_(false) {
    setVariantValue(static_cast<double>(value));
}

FFIValue::FFIValue(double value) : type_(FLOAT), pointerOwned_(false) {
    setVariantValue(value);
}

FFIValue::FFIValue(const std::string& value) : type_(STRING), pointerOwned_(false) {
    setVariantValue(value);
}

FFIValue::FFIValue(const char* value) : type_(STRING), pointerOwned_(false) {
    setVariantValue(std::string(value ? value : ""));
}

FFIValue::FFIValue(const std::vector<FFIValue>& value) : type_(ARRAY), pointerOwned_(false) {
    setVariantValue(value);
}

FFIValue::FFIValue(const std::unordered_map<std::string, FFIValue>& value) : type_(OBJECT), pointerOwned_(false) {
    setVariantValue(value);
}

FFIValue::FFIValue(void* pointer, const std::string& typeName) 
    : type_(POINTER), pointerTypeName_(typeName), pointerOwned_(false) {
    setVariantValue(pointer);
}

FFIValue::FFIValue(const FFIValue& other) : pointerOwned_(false) {
    copyFrom(other);
}

FFIValue::FFIValue(FFIValue&& other) noexcept : pointerOwned_(false) {
    moveFrom(std::move(other));
}

FFIValue& FFIValue::operator=(const FFIValue& other) {
    if (this != &other) {
        cleanup();
        copyFrom(other);
    }
    return *this;
}

FFIValue& FFIValue::operator=(FFIValue&& other) noexcept {
    if (this != &other) {
        cleanup();
        moveFrom(std::move(other));
    }
    return *this;
}

FFIValue::~FFIValue() {
    cleanup();
}

void FFIValue::cleanup() {
    if (type_ == POINTER && pointerOwned_) {
        void* ptr = std::get<void*>(value_);
        if (ptr) {
            std::free(ptr);
        }
    }
    value_ = std::monostate{};
    type_ = UNDEFINED;
    pointerTypeName_.clear();
    pointerOwned_ = false;
    functionCallback_ = nullptr;
    errorMessage_.clear();
}

void FFIValue::copyFrom(const FFIValue& other) {
    type_ = other.type_;
    value_ = other.value_;
    pointerTypeName_ = other.pointerTypeName_;
    pointerOwned_ = false; // Don't transfer ownership
    functionCallback_ = other.functionCallback_;
    errorMessage_ = other.errorMessage_;
}

void FFIValue::moveFrom(FFIValue&& other) {
    type_ = other.type_;
    value_ = std::move(other.value_);
    pointerTypeName_ = std::move(other.pointerTypeName_);
    pointerOwned_ = other.pointerOwned_;
    functionCallback_ = std::move(other.functionCallback_);
    errorMessage_ = std::move(other.errorMessage_);
    
    other.type_ = UNDEFINED;
    other.pointerOwned_ = false;
}

// Value extraction methods
bool FFIValue::asBoolean() const {
    switch (type_) {
        case BOOLEAN: return std::get<bool>(value_);
        case INTEGER: return std::get<int64_t>(value_) != 0;
        case FLOAT: return std::get<double>(value_) != 0.0;
        case STRING: return !std::get<std::string>(value_).empty();
        case ARRAY: return !std::get<std::vector<FFIValue>>(value_).empty();
        case OBJECT: return !std::get<std::unordered_map<std::string, FFIValue>>(value_).empty();
        default: return false;
    }
}

int32_t FFIValue::asInt32() const {
    switch (type_) {
        case INTEGER: return static_cast<int32_t>(std::get<int64_t>(value_));
        case FLOAT: return static_cast<int32_t>(std::get<double>(value_));
        case BOOLEAN: return std::get<bool>(value_) ? 1 : 0;
        case STRING: {
            try {
                return std::stoi(std::get<std::string>(value_));
            } catch (...) {
                return 0;
            }
        }
        default: return 0;
    }
}

int64_t FFIValue::asInt64() const {
    switch (type_) {
        case INTEGER: return std::get<int64_t>(value_);
        case FLOAT: return static_cast<int64_t>(std::get<double>(value_));
        case BOOLEAN: return std::get<bool>(value_) ? 1 : 0;
        case STRING: {
            try {
                return std::stoll(std::get<std::string>(value_));
            } catch (...) {
                return 0;
            }
        }
        default: return 0;
    }
}

float FFIValue::asFloat() const {
    switch (type_) {
        case FLOAT: return static_cast<float>(std::get<double>(value_));
        case INTEGER: return static_cast<float>(std::get<int64_t>(value_));
        case BOOLEAN: return std::get<bool>(value_) ? 1.0f : 0.0f;
        case STRING: {
            try {
                return std::stof(std::get<std::string>(value_));
            } catch (...) {
                return 0.0f;
            }
        }
        default: return 0.0f;
    }
}

double FFIValue::asDouble() const {
    switch (type_) {
        case FLOAT: return std::get<double>(value_);
        case INTEGER: return static_cast<double>(std::get<int64_t>(value_));
        case BOOLEAN: return std::get<bool>(value_) ? 1.0 : 0.0;
        case STRING: {
            try {
                return std::stod(std::get<std::string>(value_));
            } catch (...) {
                return 0.0;
            }
        }
        default: return 0.0;
    }
}

std::string FFIValue::asString() const {
    switch (type_) {
        case STRING: return std::get<std::string>(value_);
        case INTEGER: return std::to_string(std::get<int64_t>(value_));
        case FLOAT: return std::to_string(std::get<double>(value_));
        case BOOLEAN: return std::get<bool>(value_) ? "true" : "false";
        case NULL_VALUE: return "null";
        case UNDEFINED: return "undefined";
        case ARRAY: return "[Array]";
        case OBJECT: return "[Object]";
        case POINTER: return "[Pointer]";
        case FUNCTION: return "[Function]";
        case BINARY_DATA: return "[BinaryData]";
        default: return "";
    }
}

const std::vector<FFIValue>& FFIValue::asArray() const {
    static const std::vector<FFIValue> empty;
    if (type_ == ARRAY) {
        return std::get<std::vector<FFIValue>>(value_);
    }
    return empty;
}

std::vector<FFIValue>& FFIValue::asArray() {
    if (type_ != ARRAY) {
        type_ = ARRAY;
        setVariantValue(std::vector<FFIValue>{});
    }
    return std::get<std::vector<FFIValue>>(value_);
}

const std::unordered_map<std::string, FFIValue>& FFIValue::asObject() const {
    static const std::unordered_map<std::string, FFIValue> empty;
    if (type_ == OBJECT) {
        return std::get<std::unordered_map<std::string, FFIValue>>(value_);
    }
    return empty;
}

std::unordered_map<std::string, FFIValue>& FFIValue::asObject() {
    if (type_ != OBJECT) {
        type_ = OBJECT;
        setVariantValue(std::unordered_map<std::string, FFIValue>{});
    }
    return std::get<std::unordered_map<std::string, FFIValue>>(value_);
}

void* FFIValue::asPointer() const {
    if (type_ == POINTER) {
        return std::get<void*>(value_);
    }
    return nullptr;
}

const std::vector<uint8_t>& FFIValue::asBinaryData() const {
    static const std::vector<uint8_t> empty;
    if (type_ == BINARY_DATA) {
        return std::get<std::vector<uint8_t>>(value_);
    }
    return empty;
}

// Array operations
size_t FFIValue::arraySize() const {
    if (type_ == ARRAY) {
        return std::get<std::vector<FFIValue>>(value_).size();
    }
    return 0;
}

FFIValue FFIValue::arrayGet(size_t index) const {
    if (type_ == ARRAY) {
        const auto& arr = std::get<std::vector<FFIValue>>(value_);
        if (index < arr.size()) {
            return arr[index];
        }
    }
    return FFIValue();
}

void FFIValue::arraySet(size_t index, const FFIValue& value) {
    if (type_ != ARRAY) {
        type_ = ARRAY;
        setVariantValue(std::vector<FFIValue>{});
    }
    auto& arr = std::get<std::vector<FFIValue>>(value_);
    if (index >= arr.size()) {
        arr.resize(index + 1);
    }
    arr[index] = value;
}

void FFIValue::arrayPush(const FFIValue& value) {
    if (type_ != ARRAY) {
        type_ = ARRAY;
        setVariantValue(std::vector<FFIValue>{});
    }
    auto& arr = std::get<std::vector<FFIValue>>(value_);
    arr.push_back(value);
}

FFIValue FFIValue::arrayPop() {
    if (type_ == ARRAY) {
        auto& arr = std::get<std::vector<FFIValue>>(value_);
        if (!arr.empty()) {
            FFIValue result = arr.back();
            arr.pop_back();
            return result;
        }
    }
    return FFIValue();
}

void FFIValue::arrayClear() {
    if (type_ == ARRAY) {
        std::get<std::vector<FFIValue>>(value_).clear();
    }
}

// Object operations
size_t FFIValue::objectSize() const {
    if (type_ == OBJECT) {
        return std::get<std::unordered_map<std::string, FFIValue>>(value_).size();
    }
    return 0;
}

bool FFIValue::objectHas(const std::string& key) const {
    if (type_ == OBJECT) {
        const auto& obj = std::get<std::unordered_map<std::string, FFIValue>>(value_);
        return obj.find(key) != obj.end();
    }
    return false;
}

FFIValue FFIValue::objectGet(const std::string& key) const {
    if (type_ == OBJECT) {
        const auto& obj = std::get<std::unordered_map<std::string, FFIValue>>(value_);
        auto it = obj.find(key);
        if (it != obj.end()) {
            return it->second;
        }
    }
    return FFIValue();
}

void FFIValue::objectSet(const std::string& key, const FFIValue& value) {
    if (type_ != OBJECT) {
        type_ = OBJECT;
        setVariantValue(std::unordered_map<std::string, FFIValue>{});
    }
    auto& obj = std::get<std::unordered_map<std::string, FFIValue>>(value_);
    obj[key] = value;
}

void FFIValue::objectRemove(const std::string& key) {
    if (type_ == OBJECT) {
        auto& obj = std::get<std::unordered_map<std::string, FFIValue>>(value_);
        obj.erase(key);
    }
}

std::vector<std::string> FFIValue::objectKeys() const {
    std::vector<std::string> keys;
    if (type_ == OBJECT) {
        const auto& obj = std::get<std::unordered_map<std::string, FFIValue>>(value_);
        keys.reserve(obj.size());
        for (const auto& pair : obj) {
            keys.push_back(pair.first);
        }
    }
    return keys;
}

void FFIValue::objectClear() {
    if (type_ == OBJECT) {
        std::get<std::unordered_map<std::string, FFIValue>>(value_).clear();
    }
}

// String representation
std::string FFIValue::toString() const {
    return asString();
}

std::string FFIValue::getTypeName() const {
    switch (type_) {
        case UNDEFINED: return "undefined";
        case NULL_VALUE: return "null";
        case BOOLEAN: return "boolean";
        case INTEGER: return "integer";
        case FLOAT: return "float";
        case STRING: return "string";
        case ARRAY: return "array";
        case OBJECT: return "object";
        case FUNCTION: return "function";
        case POINTER: return "pointer";
        case BINARY_DATA: return "binary";
        default: return "unknown";
    }
}

// Comparison operators
bool FFIValue::operator==(const FFIValue& other) const {
    if (type_ != other.type_) return false;
    
    switch (type_) {
        case BOOLEAN: return std::get<bool>(value_) == std::get<bool>(other.value_);
        case INTEGER: return std::get<int64_t>(value_) == std::get<int64_t>(other.value_);
        case FLOAT: return std::get<double>(value_) == std::get<double>(other.value_);
        case STRING: return std::get<std::string>(value_) == std::get<std::string>(other.value_);
        case ARRAY: return std::get<std::vector<FFIValue>>(value_) == std::get<std::vector<FFIValue>>(other.value_);
        case OBJECT: return std::get<std::unordered_map<std::string, FFIValue>>(value_) == std::get<std::unordered_map<std::string, FFIValue>>(other.value_);
        case POINTER: return std::get<void*>(value_) == std::get<void*>(other.value_);
        case BINARY_DATA: return std::get<std::vector<uint8_t>>(value_) == std::get<std::vector<uint8_t>>(other.value_);
        default: return true; // UNDEFINED, NULL_VALUE are equal to themselves
    }
}

bool FFIValue::operator!=(const FFIValue& other) const {
    return !(*this == other);
}

// Factory methods
FFIValue FFIValue::createUndefined() {
    return FFIValue();
}

FFIValue FFIValue::createNull() {
    FFIValue value;
    value.type_ = NULL_VALUE;
    return value;
}

FFIValue FFIValue::createArray(size_t size) {
    std::vector<FFIValue> arr(size);
    return FFIValue(arr);
}

FFIValue FFIValue::createObject() {
    std::unordered_map<std::string, FFIValue> obj;
    return FFIValue(obj);
}

FFIValue FFIValue::createBinaryData(const void* data, size_t size) {
    FFIValue value;
    value.type_ = BINARY_DATA;
    std::vector<uint8_t> binary(size);
    if (data && size > 0) {
        std::memcpy(binary.data(), data, size);
    }
    value.setVariantValue(binary);
    return value;
}

// Template helper methods
template<typename T>
T FFIValue::getVariantValue() const {
    try {
        return std::get<T>(value_);
    } catch (const std::bad_variant_access&) {
        return T{};
    }
}

template<typename T>
void FFIValue::setVariantValue(const T& value) {
    value_ = value;
}

} // namespace ffi 