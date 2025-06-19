#ifndef RUNTIME_H
#define RUNTIME_H

#include <variant>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

namespace interpreter {

    // Forward declarations
    class Function;
    class Class;

    // Define the runtime value types using std::variant
    using Value = std::variant<
        std::monostate,  // null
        bool,
        int64_t,
        double,
        std::string,
        std::shared_ptr<Function>,
        std::shared_ptr<Class>,
        std::vector<Value>,
        std::unordered_map<std::string, Value>
    >;

    // Function class to represent functions
    class Function {
    public:
        Function(const std::string& name, const std::vector<std::string>& params, const std::vector<Value>& body)
            : name(name), params(params), body(body) {}

        const std::string& getName() const { return name; }
        const std::vector<std::string>& getParams() const { return params; }
        const std::vector<Value>& getBody() const { return body; }

    private:
        std::string name;
        std::vector<std::string> params;
        std::vector<Value> body;
    };

    // Class class to represent classes
    class Class {
    public:
        Class(const std::string& name, const std::unordered_map<std::string, Value>& methods)
            : name(name), methods(methods) {}

        const std::string& getName() const { return name; }
        const std::unordered_map<std::string, Value>& getMethods() const { return methods; }

    private:
        std::string name;
        std::unordered_map<std::string, Value> methods;
    };

} // namespace interpreter

#endif // RUNTIME_H 