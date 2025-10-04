# Lexer and Parser Enhancement Documentation

## Overview

The Tocin lexer and parser form the foundation of the compiler pipeline. This document describes the advanced implementation details and enhancements.

## Lexer Enhancements

### Current Implementation

The lexer (`src/lexer/lexer.cpp`) provides:
- **Indentation-aware tokenization** - Python-style significant whitespace
- **Unicode support** - Full UTF-8 identifier and string support
- **Template literals** - String interpolation with embedded expressions
- **Error recovery** - Continue parsing after errors with synchronization
- **Context tracking** - Line and column information for all tokens

### Advanced Features

#### 1. Indentation Handling

The lexer tracks indentation levels and generates INDENT/DEDENT tokens:

```cpp
void Lexer::handleIndentation() {
    int spaces = 0;
    while (peek() == ' ') {
        spaces++;
        advance();
    }
    
    int newIndentLevel = spaces / indentSize;
    
    if (newIndentLevel > indentLevel) {
        // Generate INDENT tokens
        for (int i = 0; i < (newIndentLevel - indentLevel); i++) {
            tokens.push_back(makeToken(TokenType::INDENT));
        }
    } else if (newIndentLevel < indentLevel) {
        // Generate DEDENT tokens
        for (int i = 0; i < (indentLevel - newIndentLevel); i++) {
            tokens.push_back(makeToken(TokenType::DEDENT));
        }
    }
    
    indentLevel = newIndentLevel;
}
```

**Enhancement**: Configurable indentation (2, 4, or 8 spaces)

#### 2. String Interpolation

Template literals support embedded expressions:

```to
let name = "World";
let greeting = f"Hello, {name}!";  // Result: "Hello, World!"
```

**Lexer Implementation**:
```cpp
void Lexer::scanTemplateLiteral() {
    std::string value;
    bool inExpression = false;
    int braceDepth = 0;
    
    while (!isAtEnd() && (peek() != '"' || inExpression)) {
        if (peek() == '{' && peekNext() != '{') {
            // Start of expression
            inExpression = true;
            braceDepth = 1;
            // Emit string part
            tokens.push_back(makeToken(TokenType::STRING, value));
            value.clear();
            advance(); // consume {
            
            // Tokenize expression
            while (braceDepth > 0) {
                scanToken();
                if (peek() == '{') braceDepth++;
                if (peek() == '}') braceDepth--;
            }
            
            inExpression = false;
        } else {
            value += advance();
        }
    }
    
    // Emit final string part
    tokens.push_back(makeToken(TokenType::STRING, value));
}
```

#### 3. Error Recovery

The lexer continues after errors and provides helpful messages:

```cpp
void Lexer::reportError(error::ErrorCode code, const std::string& message) {
    errorHandler.reportError(code, message, filename, line, column, 
                            error::ErrorSeverity::ERROR);
    errorCount++;
    
    if (errorCount >= maxErrors) {
        throw std::runtime_error("Too many lexer errors");
    }
    
    // Skip to next line or synchronization point
    synchronize();
}
```

**Enhancement**: Suggest fixes for common errors

#### 4. Brace-Aware Indentation

The lexer disables indentation tracking inside braces for flexibility:

```to
let data = {
    name: "Alice",
        age: 30,  // Indentation inside braces is ignored
    city: "NYC"
}
```

**Implementation**:
```cpp
void Lexer::scanToken() {
    // Track brace depth
    if (peek() == '{') {
        braceDepth++;
    } else if (peek() == '}') {
        braceDepth--;
    }
    
    // Only process indentation at line start when not in braces
    if (atLineStart && braceDepth == 0) {
        handleIndentation();
    }
}
```

### Proposed Enhancements

#### 1. Performance Optimization
- Use rope data structure for large files
- Implement lookahead buffer for better performance
- Add token caching for repeated scans

#### 2. Better Unicode Support
- Full normalization of identifiers
- Emoji support in strings
- Right-to-left language support

#### 3. Enhanced Error Messages
```to
// Current:
Error: Unexpected character '@' at line 5, column 10

// Enhanced:
Error: Unexpected character '@' at line 5, column 10
  let result @ 42;
             ^
  Did you mean '=' for assignment?
  Or '==' for comparison?
```

## Parser Enhancements

### Current Implementation

The parser (`src/parser/parser.cpp`) uses:
- **Recursive descent** - Simple and maintainable
- **Pratt parsing** - For expression precedence
- **Error recovery** - Synchronization after errors
- **Full language support** - All Tocin features

### Advanced Features

#### 1. Operator Precedence

Implemented using Pratt parsing for expressions:

```cpp
ast::ExprPtr Parser::parseExpression(int precedence) {
    auto left = parsePrimary();
    
    while (precedence < getPrecedence(peek().type)) {
        auto op = advance();
        auto right = parseExpression(getPrecedence(op.type));
        left = std::make_unique<ast::BinaryExpr>(left, op, right);
    }
    
    return left;
}

int Parser::getPrecedence(lexer::TokenType type) {
    switch (type) {
        case TokenType::STAR:
        case TokenType::SLASH: return 60;
        case TokenType::PLUS:
        case TokenType::MINUS: return 50;
        case TokenType::LESS:
        case TokenType::GREATER: return 40;
        case TokenType::EQUAL_EQUAL: return 30;
        case TokenType::AND: return 20;
        case TokenType::OR: return 10;
        default: return 0;
    }
}
```

#### 2. Error Recovery

Synchronization points for error recovery:

```cpp
void Parser::synchronize() {
    advance();
    
    while (!isAtEnd()) {
        if (previous().type == TokenType::SEMI_COLON) {
            return;
        }
        
        switch (peek().type) {
            case TokenType::CLASS:
            case TokenType::DEF:
            case TokenType::LET:
            case TokenType::CONST:
            case TokenType::FOR:
            case TokenType::IF:
            case TokenType::WHILE:
            case TokenType::RETURN:
                return;
            default:
                advance();
        }
    }
}
```

#### 3. Pattern Matching

Advanced pattern matching support:

```to
match value {
    0 => print("zero"),
    1 | 2 | 3 => print("small"),
    x if x > 10 => print("large"),
    [first, ...rest] => print(f"list: {first}"),
    { name, age } => print(f"{name} is {age}"),
    _ => print("other")
}
```

**Parser Implementation**:
```cpp
ast::StmtPtr Parser::matchStmt() {
    consume(TokenType::MATCH, "Expected 'match'");
    auto expression = parseExpression();
    consume(TokenType::LEFT_BRACE, "Expected '{' after match expression");
    
    std::vector<ast::MatchCase> cases;
    
    while (!check(TokenType::RIGHT_BRACE) && !isAtEnd()) {
        auto pattern = parsePattern();
        
        // Optional guard
        ast::ExprPtr guard = nullptr;
        if (match(TokenType::IF)) {
            guard = parseExpression();
        }
        
        consume(TokenType::ARROW, "Expected '=>' in match case");
        auto body = parseStatement();
        
        cases.push_back(ast::MatchCase{pattern, guard, body});
        
        // Optional comma
        match(TokenType::COMMA);
    }
    
    consume(TokenType::RIGHT_BRACE, "Expected '}' after match cases");
    
    return std::make_unique<ast::MatchStmt>(expression, cases);
}
```

#### 4. LINQ-Style Operations

Parse method chaining for collections:

```to
let result = users
    .where(u => u.age > 18)
    .select(u => u.name)
    .orderBy(name => name.length)
    .take(10);
```

**Parser handles this as chained method calls with lambda expressions.**

### Proposed Enhancements

#### 1. Incremental Parsing
- Parse only changed portions of file
- Useful for IDE integration
- Significant performance improvement for large files

#### 2. Better Error Messages
```to
// Current:
Error: Expected '}' at line 15

// Enhanced:
Error: Unclosed brace starting at line 10
  10 | def calculate() {
     |                 ^
  15 |   return result;
  16 | // Missing closing brace here

Suggestion: Add '}' before line 16
```

#### 3. Semantic Analysis During Parsing
- Early error detection
- Better error messages
- Type inference hints

## AST (Abstract Syntax Tree) Enhancements

### Current Implementation

The AST (`src/ast/ast.h`, `src/ast/ast.cpp`) provides:
- Complete representation of all language constructs
- Visitor pattern for traversal
- Type information attached to nodes
- Location information for error reporting

### Node Types

#### Expression Nodes
```cpp
class Expr {
public:
    virtual void accept(Visitor& visitor) = 0;
    TypePtr type;  // Inferred type
    SourceLocation location;
};

class BinaryExpr : public Expr {
    ExprPtr left;
    Token op;
    ExprPtr right;
};

class CallExpr : public Expr {
    ExprPtr callee;
    std::vector<ExprPtr> arguments;
};

class LambdaExpr : public Expr {
    std::vector<Parameter> parameters;
    StmtPtr body;
    TypePtr returnType;
};
```

#### Statement Nodes
```cpp
class Stmt {
public:
    virtual void accept(Visitor& visitor) = 0;
    SourceLocation location;
};

class FunctionDeclStmt : public Stmt {
    Token name;
    std::vector<Parameter> parameters;
    TypePtr returnType;
    StmtPtr body;
    bool isAsync;
};

class ClassDeclStmt : public Stmt {
    Token name;
    std::vector<Token> traits;
    std::vector<PropertyDecl> properties;
    std::vector<FunctionDeclStmt> methods;
};
```

### AST Transformations

#### 1. Desugaring

Transform syntactic sugar into core constructs:

```to
// Before desugaring
for item in collection {
    print(item);
}

// After desugaring
{
    let __iter = collection.iterator();
    while (__iter.has_next()) {
        let item = __iter.next();
        print(item);
    }
}
```

#### 2. Lambda Lifting

Convert lambdas to top-level functions:

```to
// Before
let add_n = (n) => (x) => x + n;

// After
def __lambda_1(n, x) {
    return x + n;
}

def __lambda_0(n) {
    return (x) => __lambda_1(n, x);
}
```

### Proposed Enhancements

#### 1. AST Optimization Passes
- Constant folding
- Dead code elimination
- Common subexpression elimination

#### 2. AST Caching
- Serialize parsed AST to disk
- Faster recompilation
- Dependency tracking

#### 3. AST Visualization
- Generate GraphViz output
- Interactive AST browser
- Debugging tool

## IR (Intermediate Representation) Generation

### Current Status

The IR generator (`src/codegen/ir_generator.cpp`) converts AST to LLVM IR:
- ✅ Complete coverage of basic constructs
- ✅ Function definitions with parameters
- ✅ Control flow (if, while, for)
- ✅ Expression evaluation
- ✅ Type conversions

### Completion Roadmap

#### 1. Advanced Expression Support

**Pattern Matching**:
```cpp
llvm::Value* IRGenerator::generateMatchExpr(ast::MatchExpr* expr) {
    // Generate switch/case for simple patterns
    // Generate if-else chain for complex patterns
    // Support guards with additional conditions
}
```

**LINQ Operations**:
```cpp
llvm::Value* IRGenerator::generateLINQExpr(ast::LINQExpr* expr) {
    // Generate iterator-based code
    // Optimize for common patterns
    // Support lazy evaluation
}
```

#### 2. Async/Await

```cpp
llvm::Function* IRGenerator::generateAsyncFunction(ast::FunctionDeclStmt* stmt) {
    // Transform to state machine
    // Generate coroutine code
    // Handle await points
}
```

#### 3. Goroutine Support

```cpp
void IRGenerator::generateGoStmt(ast::GoStmt* stmt) {
    // Create fiber/coroutine
    // Schedule on runtime scheduler
    // Pass closure if needed
}
```

#### 4. Channel Operations

```cpp
llvm::Value* IRGenerator::generateChannelSend(ast::ChannelSendExpr* expr) {
    // Generate channel.send() call
    // Handle blocking semantics
}

llvm::Value* IRGenerator::generateChannelReceive(ast::ChannelReceiveExpr* expr) {
    // Generate channel.receive() call
    // Handle blocking semantics
}
```

### Optimization Opportunities

#### 1. Tail Call Optimization
- Detect tail calls
- Transform to jumps
- Enable unbounded recursion

#### 2. Loop Vectorization
- Detect vectorizable loops
- Generate SIMD instructions
- Significant performance gains

#### 3. Inlining
- Inline small functions
- Cross-module inlining
- Profile-guided decisions

## Testing and Validation

### Parser Tests
```to
// Test file: tests/parser/expressions.to
// Test binary operators
assert_ast("1 + 2", BinaryExpr(1, PLUS, 2));

// Test precedence
assert_ast("1 + 2 * 3", BinaryExpr(1, PLUS, BinaryExpr(2, STAR, 3)));

// Test associativity
assert_ast("1 - 2 - 3", BinaryExpr(BinaryExpr(1, MINUS, 2), MINUS, 3));
```

### IR Generation Tests
```cpp
// Test IR generation
TEST(IRGenerator, SimpleFunction) {
    auto ast = parse("def add(a: int, b: int) -> int { return a + b; }");
    auto ir = generator.generate(ast);
    
    ASSERT_TRUE(ir->getFunction("add") != nullptr);
    ASSERT_EQ(ir->getFunction("add")->arg_size(), 2);
}
```

## Future Work

### Short Term
1. Complete pattern matching IR generation
2. Optimize common expression patterns
3. Add more comprehensive error messages

### Medium Term
1. Implement incremental parsing
2. Add AST caching
3. Optimize IR generation for performance

### Long Term
1. Self-hosting (write compiler in Tocin)
2. Custom IR optimizations
3. Multiple backend targets (WASM, ARM, etc.)

## Conclusion

The lexer, parser, AST, and IR generation components form a robust foundation for the Tocin compiler. The current implementation supports all major language features, with clear paths for optimization and enhancement.
