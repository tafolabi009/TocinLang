#ifndef ENVIRONMENT_H
#define ENVIRONMENT_H

#include "Runtime.h"
#include <unordered_map>
#include <string>
#include <memory>

namespace interpreter {

    class Environment {
    public:
        Environment() = default;
        explicit Environment(std::shared_ptr<Environment> enclosing) : enclosing(enclosing) {}

        void define(const std::string& name, const Value& value);
        Value get(const std::string& name);
        void assign(const std::string& name, const Value& value);

    private:
        std::unordered_map<std::string, Value> values;
        std::shared_ptr<Environment> enclosing;
    };

} // namespace interpreter

#endif // ENVIRONMENT_H 