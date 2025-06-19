#pragma once

#include "ast.h"

namespace ast
{

/**
 * @brief Access levels for property accessors
 */
enum class AccessorVisibility
{
    SAME_AS_PROPERTY, // Use the same visibility as the property
    PUBLIC,           // Publicly accessible
    PRIVATE,          // Accessible only within the class
    PROTECTED         // Accessible within class and subclasses
};

/**
 * @brief Base class for property accessors (getters and setters)
 */
class PropertyAccessor
{
public:
    AccessorVisibility visibility;
    StmtPtr body;

    PropertyAccessor(AccessorVisibility visibility, StmtPtr body)
        : visibility(visibility), body(body) {}

    virtual ~PropertyAccessor() = default;
};

/**
 * @brief Getter implementation for a property
 */
class PropertyGetter : public PropertyAccessor
{
public:
    PropertyGetter(AccessorVisibility visibility, StmtPtr body)
        : PropertyAccessor(visibility, body) {}
};

/**
 * @brief Setter implementation for a property
 */
class PropertySetter : public PropertyAccessor
{
public:
    std::string parameterName; // Optional parameter name for the setter

    PropertySetter(AccessorVisibility visibility, StmtPtr body, std::string parameterName = "value")
        : PropertyAccessor(visibility, body), parameterName(parameterName) {}
};

/**
 * @brief AST node for a property declaration in a class
 * 
 * Properties provide an abstraction over fields, allowing control over
 * how values are set and retrieved, similar to C# properties.
 */
class PropertyStmt : public Stmt
{
public:
    std::string name;             // Property name
    TypePtr type;                 // Property type
    ExprPtr initializer;          // Optional initial value
    bool isReadOnly;              // If true, no setter is allowed
    bool isAutoProperty;          // If true, accessors are auto-implemented (no explicit body)
    AccessorVisibility visibility; // Property visibility
    
    std::shared_ptr<PropertyGetter> getter; // Getter implementation
    std::shared_ptr<PropertySetter> setter; // Setter implementation (nullptr if read-only)

    // Backing field name (generated for auto-properties)
    std::string backingFieldName;

    PropertyStmt(std::string name, TypePtr type, 
                 ExprPtr initializer = nullptr,
                 bool isReadOnly = false,
                 bool isAutoProperty = true,
                 AccessorVisibility visibility = AccessorVisibility::PUBLIC)
        : name(name), type(type), initializer(initializer),
          isReadOnly(isReadOnly), isAutoProperty(isAutoProperty),
          visibility(visibility)
    {
        // Generate a default backing field name
        backingFieldName = "_" + name;
    }

    void setGetter(std::shared_ptr<PropertyGetter> getter)
    {
        this->getter = getter;
        isAutoProperty = false;
    }

    void setSetter(std::shared_ptr<PropertySetter> setter)
    {
        if (!isReadOnly)
        {
            this->setter = setter;
            isAutoProperty = false;
        }
    }

    virtual void accept(Visitor &visitor) override
    {
        visitor.visitPropertyStmt(this);
    }
};

/**
 * @brief Extension to the Visitor interface for property statements
 */
class PropertyVisitor
{
public:
    virtual void visitPropertyStmt(PropertyStmt *stmt) = 0;
};

} // namespace ast 
