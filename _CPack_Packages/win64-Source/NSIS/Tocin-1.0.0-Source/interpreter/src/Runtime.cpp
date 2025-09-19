#include "Runtime.h"

namespace interpreter {

    // Function implementation
    Function::Function(const std::string& name, const std::vector<std::string>& params, const std::vector<Value>& body)
        : name(name), params(params), body(body) {}

    const std::string& Function::getName() const { return name; }
    const std::vector<std::string>& Function::getParams() const { return params; }
    const std::vector<Value>& Function::getBody() const { return body; }

    // Class implementation
    Class::Class(const std::string& name, const std::unordered_map<std::string, Value>& methods)
        : name(name), methods(methods) {}

    const std::string& Class::getName() const { return name; }
    const std::unordered_map<std::string, Value>& Class::getMethods() const { return methods; }

} // namespace interpreter 