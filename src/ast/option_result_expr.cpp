#include "option_result_expr.h"

namespace ast
{

    void OptionExpr::accept(Visitor &visitor)
    {
        // Default implementation - do nothing
        // Concrete visitors should override this behavior
    }

    void ResultExpr::accept(Visitor &visitor)
    {
        // Default implementation - do nothing
        // Concrete visitors should override this behavior
    }

} // namespace ast
