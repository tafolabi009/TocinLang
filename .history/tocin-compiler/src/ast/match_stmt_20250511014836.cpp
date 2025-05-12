#include "match_stmt.h"
#include "visitor.h"

namespace ast {

// ConstructorPattern implementation
bool ConstructorPattern::bindsVariables() const {
    for (const auto& arg : arguments) {
        if (arg->bindsVariables()) {
            return true;
        }
    }
    return false;
}

std::vector<std::string> ConstructorPattern::boundVariables() const {
    std::vector<std::string> result;
    for (const auto& arg : arguments) {
        auto argVars = arg->boundVariables();
        result.insert(result.end(), argVars.begin(), argVars.end());
    }
    return result;
}

// MatchStmt visitor acceptance
void MatchStmt::accept(Visitor& visitor) {
    visitor.visitMatchStmt(this);
}

} // namespace ast 
