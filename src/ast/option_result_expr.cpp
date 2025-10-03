#include "option_result_expr.h"
#include "visitor.h"

namespace ast
{

    void OptionExpr::accept(Visitor &visitor)
    {
        // Try to cast to EnhancedVisitor which has visitOptionExpr
        if (auto* enhancedVisitor = dynamic_cast<EnhancedVisitor*>(&visitor)) {
            enhancedVisitor->visitOptionExpr(this);
        }
        // If not an EnhancedVisitor, do nothing (base visitor doesn't know about OptionExpr)
    }

    void ResultExpr::accept(Visitor &visitor)
    {
        // Try to cast to EnhancedVisitor which has visitResultExpr
        if (auto* enhancedVisitor = dynamic_cast<EnhancedVisitor*>(&visitor)) {
            enhancedVisitor->visitResultExpr(this);
        }
        // If not an EnhancedVisitor, do nothing (base visitor doesn't know about ResultExpr)
    }

} // namespace ast
