#pragma once

#include "../ast/ast.h"
#include "../error/error_handler.h"
#include <memory>
#include <vector>
#include <unordered_map>
#include <functional>
#include <string>
#include <optional>

namespace debugger {

/**
 * @brief Debugger event types
 */
enum class DebugEventType {
    BREAKPOINT_HIT,
    STEP_COMPLETE,
    EXCEPTION_THROWN,
    FUNCTION_ENTER,
    FUNCTION_EXIT,
    VARIABLE_CHANGED,
    THREAD_STARTED,
    THREAD_ENDED
};

/**
 * @brief Debugger event
 */
struct DebugEvent {
    DebugEventType type;
    std::string message;
    std::string filename;
    int line;
    int column;
    std::unordered_map<std::string, std::string> context;
    
    DebugEvent(DebugEventType t, const std::string& msg, const std::string& file = "", 
               int l = 0, int c = 0)
        : type(t), message(msg), filename(file), line(l), column(c) {}
};

/**
 * @brief Breakpoint information
 */
struct Breakpoint {
    std::string filename;
    int line;
    int column;
    bool enabled;
    std::string condition;
    int hitCount;
    std::string logMessage;
    
    Breakpoint(const std::string& file, int l, int c = 0)
        : filename(file), line(l), column(c), enabled(true), hitCount(0) {}
};

/**
 * @brief Variable information
 */
struct VariableInfo {
    std::string name;
    std::string type;
    std::string value;
    bool isConstant;
    std::string scope;
    int line;
    
    VariableInfo(const std::string& n, const std::string& t, const std::string& v)
        : name(n), type(t), value(v), isConstant(false), line(0) {}
};

/**
 * @brief Call stack frame
 */
struct CallFrame {
    std::string functionName;
    std::string filename;
    int line;
    int column;
    std::vector<VariableInfo> localVariables;
    std::vector<VariableInfo> parameters;
    
    CallFrame(const std::string& func, const std::string& file, int l, int c)
        : functionName(func), filename(file), line(l), column(c) {}
};

/**
 * @brief Debugger state
 */
enum class DebuggerState {
    RUNNING,
    PAUSED,
    STEPPING,
    STOPPED
};

/**
 * @brief Debugger interface
 */
class Debugger {
public:
    virtual ~Debugger() = default;
    
    /**
     * @brief Initialize the debugger
     */
    virtual bool initialize() = 0;
    
    /**
     * @brief Start debugging
     */
    virtual void start() = 0;
    
    /**
     * @brief Stop debugging
     */
    virtual void stop() = 0;
    
    /**
     * @brief Pause execution
     */
    virtual void pause() = 0;
    
    /**
     * @brief Resume execution
     */
    virtual void resume() = 0;
    
    /**
     * @brief Step into
     */
    virtual void stepInto() = 0;
    
    /**
     * @brief Step over
     */
    virtual void stepOver() = 0;
    
    /**
     * @brief Step out
     */
    virtual void stepOut() = 0;
    
    /**
     * @brief Continue execution
     */
    virtual void continueExecution() = 0;
    
    /**
     * @brief Set breakpoint
     */
    virtual bool setBreakpoint(const std::string& filename, int line, int column = 0) = 0;
    
    /**
     * @brief Remove breakpoint
     */
    virtual bool removeBreakpoint(const std::string& filename, int line, int column = 0) = 0;
    
    /**
     * @brief Enable/disable breakpoint
     */
    virtual bool toggleBreakpoint(const std::string& filename, int line, int column = 0) = 0;
    
    /**
     * @brief Get all breakpoints
     */
    virtual std::vector<Breakpoint> getBreakpoints() const = 0;
    
    /**
     * @brief Get current call stack
     */
    virtual std::vector<CallFrame> getCallStack() const = 0;
    
    /**
     * @brief Get variables in current scope
     */
    virtual std::vector<VariableInfo> getLocalVariables() const = 0;
    
    /**
     * @brief Get global variables
     */
    virtual std::vector<VariableInfo> getGlobalVariables() const = 0;
    
    /**
     * @brief Evaluate expression
     */
    virtual std::string evaluateExpression(const std::string& expression) = 0;
    
    /**
     * @brief Set variable value
     */
    virtual bool setVariableValue(const std::string& name, const std::string& value) = 0;
    
    /**
     * @brief Get current state
     */
    virtual DebuggerState getState() const = 0;
    
    /**
     * @brief Get current position
     */
    virtual std::optional<std::pair<std::string, int>> getCurrentPosition() const = 0;
    
    /**
     * @brief Set event callback
     */
    virtual void setEventCallback(std::function<void(const DebugEvent&)> callback) = 0;
};

/**
 * @brief LLVM-based debugger implementation
 */
class LLVMDebugger : public Debugger {
private:
    DebuggerState state;
    std::vector<Breakpoint> breakpoints;
    std::vector<CallFrame> callStack;
    std::unordered_map<std::string, std::string> variables;
    std::function<void(const DebugEvent&)> eventCallback;
    bool initialized;
    
    // LLVM-specific members
    void* executionEngine;
    void* debugInfo;
    void* symbolTable;
    
public:
    LLVMDebugger();
    ~LLVMDebugger() override;
    
    bool initialize() override;
    void start() override;
    void stop() override;
    void pause() override;
    void resume() override;
    void stepInto() override;
    void stepOver() override;
    void stepOut() override;
    void continueExecution() override;
    
    bool setBreakpoint(const std::string& filename, int line, int column = 0) override;
    bool removeBreakpoint(const std::string& filename, int line, int column = 0) override;
    bool toggleBreakpoint(const std::string& filename, int line, int column = 0) override;
    
    std::vector<Breakpoint> getBreakpoints() const override;
    std::vector<CallFrame> getCallStack() const override;
    std::vector<VariableInfo> getLocalVariables() const override;
    std::vector<VariableInfo> getGlobalVariables() const override;
    
    std::string evaluateExpression(const std::string& expression) override;
    bool setVariableValue(const std::string& name, const std::string& value) override;
    
    DebuggerState getState() const override { return state; }
    std::optional<std::pair<std::string, int>> getCurrentPosition() const override;
    
    void setEventCallback(std::function<void(const DebugEvent&)> callback) override {
        eventCallback = callback;
    }
    
private:
    void updateCallStack();
    void updateVariables();
    void notifyEvent(const DebugEvent& event);
    bool isBreakpointHit(const std::string& filename, int line, int column) const;
    void handleBreakpoint(const Breakpoint& breakpoint);
};

/**
 * @brief Debugger session manager
 */
class DebuggerSession {
private:
    std::unique_ptr<Debugger> debugger;
    std::string programPath;
    std::vector<std::string> arguments;
    bool attached;
    
public:
    DebuggerSession();
    ~DebuggerSession();
    
    /**
     * @brief Start debugging a program
     */
    bool startProgram(const std::string& path, const std::vector<std::string>& args = {});
    
    /**
     * @brief Attach to running process
     */
    bool attachToProcess(int pid);
    
    /**
     * @brief Detach from process
     */
    void detach();
    
    /**
     * @brief Get debugger instance
     */
    Debugger* getDebugger() const { return debugger.get(); }
    
    /**
     * @brief Check if session is active
     */
    bool isActive() const { return attached; }
    
    /**
     * @brief Get program path
     */
    const std::string& getProgramPath() const { return programPath; }
    
    /**
     * @brief Get program arguments
     */
    const std::vector<std::string>& getArguments() const { return arguments; }
};

/**
 * @brief Debugger command interface
 */
class DebuggerCommand {
public:
    virtual ~DebuggerCommand() = default;
    virtual std::string execute(Debugger& debugger, const std::vector<std::string>& args) = 0;
    virtual std::string getHelp() const = 0;
};

/**
 * @brief Built-in debugger commands
 */
namespace commands {
    
    class BreakCommand : public DebuggerCommand {
    public:
        std::string execute(Debugger& debugger, const std::vector<std::string>& args) override;
        std::string getHelp() const override { return "break <file>:<line> - Set breakpoint"; }
    };
    
    class ContinueCommand : public DebuggerCommand {
    public:
        std::string execute(Debugger& debugger, const std::vector<std::string>& args) override;
        std::string getHelp() const override { return "continue - Continue execution"; }
    };
    
    class StepCommand : public DebuggerCommand {
    public:
        std::string execute(Debugger& debugger, const std::vector<std::string>& args) override;
        std::string getHelp() const override { return "step [into|over|out] - Step execution"; }
    };
    
    class VariablesCommand : public DebuggerCommand {
    public:
        std::string execute(Debugger& debugger, const std::vector<std::string>& args) override;
        std::string getHelp() const override { return "variables - Show local variables"; }
    };
    
    class StackCommand : public DebuggerCommand {
    public:
        std::string execute(Debugger& debugger, const std::vector<std::string>& args) override;
        std::string getHelp() const override { return "stack - Show call stack"; }
    };
    
    class EvaluateCommand : public DebuggerCommand {
    public:
        std::string execute(Debugger& debugger, const std::vector<std::string>& args) override;
        std::string getHelp() const override { return "eval <expression> - Evaluate expression"; }
    };
    
    class SetCommand : public DebuggerCommand {
    public:
        std::string execute(Debugger& debugger, const std::vector<std::string>& args) override;
        std::string getHelp() const override { return "set <variable> = <value> - Set variable"; }
    };
    
    class BreakpointsCommand : public DebuggerCommand {
    public:
        std::string execute(Debugger& debugger, const std::vector<std::string>& args) override;
        std::string getHelp() const override { return "breakpoints - List breakpoints"; }
    };
    
    class HelpCommand : public DebuggerCommand {
    public:
        std::string execute(Debugger& debugger, const std::vector<std::string>& args) override;
        std::string getHelp() const override { return "help - Show this help"; }
    };
    
    class QuitCommand : public DebuggerCommand {
    public:
        std::string execute(Debugger& debugger, const std::vector<std::string>& args) override;
        std::string getHelp() const override { return "quit - Exit debugger"; }
    };
}

/**
 * @brief Debugger command processor
 */
class DebuggerCommandProcessor {
private:
    std::unordered_map<std::string, std::unique_ptr<DebuggerCommand>> commands;
    Debugger* debugger;
    
public:
    DebuggerCommandProcessor(Debugger* dbg);
    
    /**
     * @brief Process a command
     */
    std::string processCommand(const std::string& command);
    
    /**
     * @brief Register a command
     */
    void registerCommand(const std::string& name, std::unique_ptr<DebuggerCommand> command);
    
    /**
     * @brief Get available commands
     */
    std::vector<std::string> getAvailableCommands() const;
    
    /**
     * @brief Get command help
     */
    std::string getCommandHelp(const std::string& command) const;
};

} // namespace debugger 