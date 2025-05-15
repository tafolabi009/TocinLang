#include "option_result_expr.h"
#include "visitor.h"

namespace ast
{

    void OptionExpr::accept(Visitor &visitor)
    {
        // Try to use EnhancedVisitor if possible, otherwise do nothing
        auto *enhancedVisitor = dynamic_cast<EnhancedVisitor *>(&visitor);
        if (enhancedVisitor)
        {
            enhancedVisitor->visitOptionExpr(this);
        }
        // If not an EnhancedVisitor, we can't call the appropriate method
    }

    void ResultExpr::accept(Visitor &visitor)
    {
        // Try to use EnhancedVisitor if possible, otherwise do nothing
        auto *enhancedVisitor = dynamic_cast<EnhancedVisitor *>(&visitor);
        if (enhancedVisitor)
        {
            enhancedVisitor->visitResultExpr(this);
        }
        // If not an EnhancedVisitor, we can't call the appropriate method
    }

} // namespace ast
