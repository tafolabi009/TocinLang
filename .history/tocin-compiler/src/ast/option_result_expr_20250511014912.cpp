#include "option_result_expr.h"
#include "visitor.h"

namespace ast
{

    void OptionExpr::accept(Visitor &visitor)
    {
        visitor.visitOptionExpr(this);
    }

    void ResultExpr::accept(Visitor &visitor)
    {
        visitor.visitResultExpr(this);
    }

} // namespace ast
