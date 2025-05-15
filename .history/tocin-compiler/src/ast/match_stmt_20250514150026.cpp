#include "match_stmt.h"
#include "visitor.h"
#include <algorithm>
#include <set>

namespace ast
{

    // ConstructorPattern implementation
    bool ConstructorPattern::bindsVariables() const
    {
        for (const auto &arg : arguments)
        {
            if (arg->bindsVariables())
            {
                return true;
            }
        }
        return false;
    }

    std::vector<std::string> ConstructorPattern::boundVariables() const
    {
        std::vector<std::string> result;
        for (const auto &arg : arguments)
        {
            auto vars = arg->boundVariables();
            result.insert(result.end(), vars.begin(), vars.end());
        }
        return result;
    }

    // TuplePattern implementation
    bool TuplePattern::bindsVariables() const
    {
        for (const auto &element : elements)
        {
            if (element->bindsVariables())
            {
                return true;
            }
        }
        return false;
    }

    std::vector<std::string> TuplePattern::boundVariables() const
    {
        std::vector<std::string> result;
        for (const auto &element : elements)
        {
            auto vars = element->boundVariables();
            result.insert(result.end(), vars.begin(), vars.end());
        }
        return result;
    }

    // StructPattern implementation
    bool StructPattern::bindsVariables() const
    {
        for (const auto &field : fields)
        {
            if (field.pattern->bindsVariables())
            {
                return true;
            }
        }
        return false;
    }

    std::vector<std::string> StructPattern::boundVariables() const
    {
        std::vector<std::string> result;
        for (const auto &field : fields)
        {
            auto vars = field.pattern->boundVariables();
            result.insert(result.end(), vars.begin(), vars.end());
        }
        return result;
    }

    // OrPattern implementation
    bool OrPattern::bindsVariables() const
    {
        // For an OR pattern to bind variables, both sides must bind the same variables
        // This is a common restriction in pattern matching to ensure determinism
        return left->bindsVariables() && right->bindsVariables() &&
               left->boundVariables() == right->boundVariables();
    }

    std::vector<std::string> OrPattern::boundVariables() const
    {
        // We arbitrarily choose the left side variables since both sides should bind the same variables
        // If they don't, bindsVariables() will return false and this won't matter
        return left->boundVariables();
    }

    // MatchStmt accept method is already defined inline in ast.h
    // Remove the redefinition here to avoid errors

} // namespace ast
