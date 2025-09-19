/**
 * @brief Runtime features for goroutine support
 *
 * Provides methods for creating, managing, and analyzing goroutines.
 */
class GoroutineSupport
{
public:
    GoroutineSupport(error::ErrorHandler &errorHandler)
        : errorHandler(errorHandler) {}

    /**
     * @brief Check if a function is eligible to be run as a goroutine
     *
     * @param function The function to check
     * @return true if the function can be used as a goroutine
     */
    bool canRunAsGoroutine(ast::FunctionStmt *function)
    {
        // All functions can be run as goroutines
        return true;
    }

    /**
     * @brief Analyze a goroutine launch to ensure it's valid
     *
     * @param function The function being launched as a goroutine
     * @param arguments The arguments passed to the function
     * @return true if the goroutine launch is valid
     */
    bool validateGoroutineLaunch(ast::ExprPtr function, const std::vector<ast::ExprPtr> &arguments)
    {
        // Validate that the function is callable
        // This is a simplified check - a real implementation would be more thorough
        if (auto varExpr = std::dynamic_pointer_cast<ast::VariableExpr>(function))
        {
            // Function reference by name - accepted
            return true;
        }
        else if (auto lambdaExpr = std::dynamic_pointer_cast<ast::LambdaExpr>(function))
        {
            // Lambda expression - accepted
            return true;
        }
        else
        {
            errorHandler.reportError(
                error::ErrorCode::C001_UNIMPLEMENTED_FEATURE,
                "Expression cannot be launched as a goroutine",
                "", 0, 0, error::ErrorSeverity::ERROR);
            return false;
        }
    }

private:
    error::ErrorHandler &errorHandler;
};

/**
 * @brief Runtime features for channel operations
 *
 * Provides methods for creating and operating on channels.
 */
class ChannelSupport
{
public:
    ChannelSupport(error::ErrorHandler &errorHandler)
        : errorHandler(errorHandler) {}

    /**
     * @brief Validate a channel send operation
     *
     * @param channel The channel expression
     * @param valueExpr The value being sent
     * @param channelType The type of the channel
     * @param valueType The type of the value
     * @return true if the send operation is valid
     */
    bool validateChannelSend(ast::ExprPtr channel, ast::ExprPtr valueExpr,
                             ast::TypePtr channelType, ast::TypePtr valueType)
    {
        // Check that it's actually a channel
        if (!ChannelType::isChannelType(channelType))
        {
            errorHandler.reportError(
                error::ErrorCode::T001_TYPE_MISMATCH,
                "Cannot send on non-channel type",
                "", 0, 0, error::ErrorSeverity::ERROR);
            return false;
        }

        // Get the channel's element type
        ast::TypePtr elementType = ChannelType::getElementType(channelType);
        if (!elementType)
        {
            errorHandler.reportError(
                error::ErrorCode::T004_UNDEFINED_TYPE,
                "Channel has undefined element type",
                "", 0, 0, error::ErrorSeverity::ERROR);
            return false;
        }

        // Check that the value type matches the channel's element type
        // This is a simplified type check - real implementation would be more thorough
        if (elementType->toString() != valueType->toString())
        {
            errorHandler.reportError(
                error::ErrorCode::T001_TYPE_MISMATCH,
                "Cannot send value of type " + valueType->toString() +
                    " on channel of type Chan<" + elementType->toString() + ">",
                "", 0, 0, error::ErrorSeverity::ERROR);
            return false;
        }

        return true;
    }

    /**
     * @brief Validate a channel receive operation
     *
     * @param channel The channel expression
     * @param channelType The type of the channel
     * @return true if the receive operation is valid
     */
    bool validateChannelReceive(ast::ExprPtr channel, ast::TypePtr channelType)
    {
        // Check that it's actually a channel
        if (!ChannelType::isChannelType(channelType))
        {
            errorHandler.reportError(
                error::ErrorCode::T001_TYPE_MISMATCH,
                "Cannot receive from non-channel type",
                "", 0, 0, error::ErrorSeverity::ERROR);
            return false;
        }

        // Get the channel's element type
        ast::TypePtr elementType = ChannelType::getElementType(channelType);
        if (!elementType)
        {
            errorHandler.reportError(
                error::ErrorCode::T004_UNDEFINED_TYPE,
                "Channel has undefined element type",
                "", 0, 0, error::ErrorSeverity::ERROR);
            return false;
        }

        return true;
    }

    /**
     * @brief Get the result type of a channel receive operation
     *
     * @param channelType The type of the channel
     * @return ast::TypePtr The type of values received from the channel
     */
    ast::TypePtr getChannelReceiveType(ast::TypePtr channelType)
    {
        return ChannelType::getElementType(channelType);
    }

private:
    error::ErrorHandler &errorHandler;
};

/**
 * @brief AST node for a goroutine launch expression
 *
 * Represents the 'go' keyword followed by a function call.
 */
class GoExpr : public ast::Expression
{
public:
    ast::ExprPtr function;               // The function to run as a goroutine
    std::vector<ast::ExprPtr> arguments; // Arguments to the function

    GoExpr(ast::ExprPtr function, std::vector<ast::ExprPtr> arguments)
        : function(function), arguments(arguments) {}

    virtual void accept(ast::Visitor &visitor) override
    {
        // This would be implemented when we add the visitor method
        // visitor.visitGoExpr(this);
    }

    virtual ast::TypePtr getType() const override
    {
        // Goroutine launches have void type
        return std::make_shared<ast::BasicType>(ast::TypeKind::VOID);
    }
};

/**
 * @brief AST node for a channel send expression
 *
 * Represents sending a value on a channel (channel <- value).
 */
class ChannelSendExpr : public ast::Expression
{
public:
    ast::ExprPtr channel; // The channel to send on
    ast::ExprPtr value;   // The value to send

    ChannelSendExpr(ast::ExprPtr channel, ast::ExprPtr value)
        : channel(channel), value(value) {}

    virtual void accept(ast::Visitor &visitor) override
    {
        // This would be implemented when we add the visitor method
        // visitor.visitChannelSendExpr(this);
    }

    virtual ast::TypePtr getType() const override
    {
        // Channel sends have void type
        return std::make_shared<ast::BasicType>(ast::TypeKind::VOID);
    }
};

/**
 * @brief AST node for a channel receive expression
 *
 * Represents receiving a value from a channel (<- channel).
 */
class ChannelReceiveExpr : public ast::Expression
{
public:
    ast::ExprPtr channel; // The channel to receive from

    ChannelReceiveExpr(ast::ExprPtr channel)
        : channel(channel) {}

    virtual void accept(ast::Visitor &visitor) override
    {
        // This would be implemented when we add the visitor method
        // visitor.visitChannelReceiveExpr(this);
    }

    virtual ast::TypePtr getType() const override
    {
        // Type depends on the channel's element type
        // This is a placeholder - in practice would use ChannelType::getElementType
        return nullptr;
    }
};

/**
 * @brief AST node for a select statement
 *
 * Represents a Go-like select statement for handling multiple channel operations.
 */
class SelectStmt : public ast::Statement
{
public:
    struct SelectCase
    {
        enum class CaseType
        {
            SEND,
            RECEIVE,
            DEFAULT
        };

        CaseType type;
        ast::ExprPtr channel = nullptr;
        ast::ExprPtr value = nullptr;  // For send cases
        std::string variableName = ""; // For receive with assignment
        ast::StmtPtr body = nullptr;
    };

    std::vector<SelectCase> cases;

    SelectStmt(std::vector<SelectCase> cases)
        : cases(cases) {}

    virtual void accept(ast::Visitor &visitor) override
    {
        // This would be implemented when we add the visitor method
        // visitor.visitSelectStmt(this);
    }
}; 
