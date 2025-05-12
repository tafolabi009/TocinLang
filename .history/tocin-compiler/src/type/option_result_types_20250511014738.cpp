#include "option_result_types.h"

namespace ast
{

    // OptionType implementation
    bool OptionType::equals(const TypePtr &other) const
    {
        if (auto otherOption = std::dynamic_pointer_cast<OptionType>(other))
        {
            return valueType->equals(otherOption->valueType);
        }
        return false;
    }

    TypePtr OptionType::clone() const
    {
        return std::make_shared<OptionType>(valueType->clone());
    }

    // ResultType implementation
    bool ResultType::equals(const TypePtr &other) const
    {
        if (auto otherResult = std::dynamic_pointer_cast<ResultType>(other))
        {
            return okType->equals(otherResult->okType) &&
                   errType->equals(otherResult->errType);
        }
        return false;
    }

    TypePtr ResultType::clone() const
    {
        return std::make_shared<ResultType>(okType->clone(), errType->clone());
    }

} // namespace ast
