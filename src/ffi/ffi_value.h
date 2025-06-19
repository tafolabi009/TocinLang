#ifndef FFI_VALUE_H
#define FFI_VALUE_H

#include <string>
#include <vector>
#include <unordered_map>
#include <variant>
#include <cstdint>
#include <stdexcept>

namespace tocin {
namespace ffi {

    enum class FFIValueType {
        Null, Int64, Double, Bool, String, Array, Object
    };

    class FFIValue {
    public:
        using ArrayType = std::vector<FFIValue>;
        using ObjectType = std::unordered_map<std::string, FFIValue>;
        using ValueType = std::variant<
            std::nullptr_t, int64_t, double, bool, std::string, ArrayType, ObjectType>;

        FFIValue() : data(std::nullptr_t{}) {}
        explicit FFIValue(int64_t i) : data(i) {}
        explicit FFIValue(double d) : data(d) {}
        explicit FFIValue(bool b) : data(b) {}
        explicit FFIValue(std::string s) : data(std::move(s)) {}
        explicit FFIValue(ArrayType arr) : data(std::move(arr)) {}
        explicit FFIValue(ObjectType obj) : data(std::move(obj)) {}

        FFIValueType getType() const { return static_cast<FFIValueType>(data.index()); }
        ValueType getValue() const { return data; }

        int64_t getInt64() const { checkType(FFIValueType::Int64); return std::get<int64_t>(data); }
        double getDouble() const { checkType(FFIValueType::Double); return std::get<double>(data); }
        bool getBool() const { checkType(FFIValueType::Bool); return std::get<bool>(data); }
        std::string getString() const { checkType(FFIValueType::String); return std::get<std::string>(data); }
        ArrayType getArray() const { checkType(FFIValueType::Array); return std::get<ArrayType>(data); }
        ObjectType getObject() const { checkType(FFIValueType::Object); return std::get<ObjectType>(data); }

    private:
        ValueType data;

        void checkType(FFIValueType expected) const {
            if (getType() != expected) {
                throw std::runtime_error("FFIValue type mismatch: expected " +
                    std::to_string(static_cast<int>(expected)));
            }
        }
    };

} // namespace ffi
} // namespace tocin

#endif // FFI_VALUE_H