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
            auto argVars = arg->boundVariables();
            result.insert(result.end(), argVars.begin(), argVars.end());
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
            auto elementVars = element->boundVariables();
            result.insert(result.end(), elementVars.begin(), elementVars.end());
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
            auto fieldVars = field.pattern->boundVariables();
            result.insert(result.end(), fieldVars.begin(), fieldVars.end());
        }
        return result;
    }

    // OrPattern implementation
    bool OrPattern::bindsVariables() const
    {
        // Both patterns must bind the same variables for consistency
        return left->bindsVariables() || right->bindsVariables();
    }

    std::vector<std::string> OrPattern::boundVariables() const
    {
        // Combine variables from both sides, removing duplicates
        auto leftVars = left->boundVariables();
        auto rightVars = right->boundVariables();
        
        std::set<std::string> uniqueVars(leftVars.begin(), leftVars.end());
        uniqueVars.insert(rightVars.begin(), rightVars.end());
        
        return std::vector<std::string>(uniqueVars.begin(), uniqueVars.end());
    }

    // MatchStmt visitor acceptance
    void MatchStmt::accept(Visitor &visitor)
    {
        visitor.visitMatchStmt(this);
    }

} // namespace ast
