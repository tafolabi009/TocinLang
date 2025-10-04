# Tocin Compiler Documentation

## Overview

This directory contains comprehensive documentation for the Tocin programming language compiler and interpreter.

## Documentation Index

### Getting Started
- **[README.md](../README.md)** - Main project README with quick start guide
- **[QUICK_START.md](../QUICK_START.md)** - Quick start guide for new users
- **[CONTRIBUTING.md](../CONTRIBUTING.md)** - How to contribute to the project

### Architecture & Design
- **[ARCHITECTURE.md](ARCHITECTURE.md)** - Complete compiler and interpreter architecture
  - System overview with diagrams
  - Component descriptions (Lexer, Parser, Type Checker, IR Generator)
  - V8 engine integration architecture
  - Goroutine scheduler design
  - Runtime system details
  - Performance characteristics

- **[LEXER_PARSER_ENHANCEMENTS.md](LEXER_PARSER_ENHANCEMENTS.md)** - Lexer and parser implementation details
  - Advanced features (indentation, string interpolation, error recovery)
  - AST structure and transformations
  - IR generation roadmap
  - Testing strategies

### User Guides
- **[INTERPRETER_GUIDE.md](INTERPRETER_GUIDE.md)** - Interpreter usage guide
  - Interactive REPL mode
  - Output format (comparison with Python)
  - JIT compilation
  - Performance tips
  - Debugging techniques

- **[INSTALLER_GUIDE.md](INSTALLER_GUIDE.md)** - Installing Tocin
  - Windows installer (NSIS)
  - Linux packages (DEB/RPM/AppImage)
  - macOS installer (DMG/Homebrew)
  - Building custom installers
  - Troubleshooting

- **[STDLIB_GUIDE.md](STDLIB_GUIDE.md)** - Standard Library documentation
  - All stdlib modules with examples
  - Math, Data Structures, Web, Database, ML, Game Dev, etc.
  - Usage patterns
  - Performance considerations

### Implementation Reports
- **[INTERPRETER_COMPLETION.md](../INTERPRETER_COMPLETION.md)** - Interpreter completion report
- **[FINAL_REPORT.md](../FINAL_REPORT.md)** - Final implementation report
- **[COMPLETION_SUMMARY.md](../COMPLETION_SUMMARY.md)** - Completion summary

## Documentation by Topic

### For Users

**I want to learn Tocin:**
1. Start with [QUICK_START.md](../QUICK_START.md)
2. Read [INTERPRETER_GUIDE.md](INTERPRETER_GUIDE.md)
3. Explore [STDLIB_GUIDE.md](STDLIB_GUIDE.md)

**I want to install Tocin:**
- Follow [INSTALLER_GUIDE.md](INSTALLER_GUIDE.md)

**I want to understand how things work:**
- Read [ARCHITECTURE.md](ARCHITECTURE.md)
- Check [LEXER_PARSER_ENHANCEMENTS.md](LEXER_PARSER_ENHANCEMENTS.md)

### For Contributors

**I want to contribute:**
1. Read [CONTRIBUTING.md](../CONTRIBUTING.md)
2. Understand [ARCHITECTURE.md](ARCHITECTURE.md)
3. Review [LEXER_PARSER_ENHANCEMENTS.md](LEXER_PARSER_ENHANCEMENTS.md)

**I want to understand the implementation:**
- [ARCHITECTURE.md](ARCHITECTURE.md) - Overall design
- [INTERPRETER_COMPLETION.md](../INTERPRETER_COMPLETION.md) - Implementation details
- [FINAL_REPORT.md](../FINAL_REPORT.md) - Features and status

### For Maintainers

**I need to build installers:**
- [INSTALLER_GUIDE.md](INSTALLER_GUIDE.md) - Installer building process

**I need to understand the codebase:**
- [ARCHITECTURE.md](ARCHITECTURE.md) - High-level architecture
- [LEXER_PARSER_ENHANCEMENTS.md](LEXER_PARSER_ENHANCEMENTS.md) - Compiler internals

## Quick Reference

### Language Features

| Feature | Status | Documentation |
|---------|--------|---------------|
| Strong Static Typing | ✅ Complete | [ARCHITECTURE.md](ARCHITECTURE.md#type-checker) |
| Type Inference | ✅ Complete | [ARCHITECTURE.md](ARCHITECTURE.md#type-system) |
| Traits | ✅ Complete | [STDLIB_GUIDE.md](STDLIB_GUIDE.md) |
| LINQ Operations | ✅ Complete | [STDLIB_GUIDE.md](STDLIB_GUIDE.md#data-structures--algorithms) |
| Null Safety | ✅ Complete | [ARCHITECTURE.md](ARCHITECTURE.md#type-checker) |
| Pattern Matching | ✅ Complete | [LEXER_PARSER_ENHANCEMENTS.md](LEXER_PARSER_ENHANCEMENTS.md#pattern-matching) |
| Async/Await | ✅ Complete | [ARCHITECTURE.md](ARCHITECTURE.md#runtime-system) |
| Goroutines | ✅ Complete | [ARCHITECTURE.md](ARCHITECTURE.md#goroutine-scheduler) |
| Channels | ✅ Complete | [ARCHITECTURE.md](ARCHITECTURE.md#concurrency-primitives) |
| FFI (C/Python/JS) | ✅ Complete | [ARCHITECTURE.md](ARCHITECTURE.md#v8-integration) |
| JIT Compilation | ✅ Complete | [INTERPRETER_GUIDE.md](INTERPRETER_GUIDE.md#jit-compilation) |
| Binary Generation | ✅ Complete | [INSTALLER_GUIDE.md](INSTALLER_GUIDE.md) |

### Compilation Pipeline

```
Source Code (.to)
    ↓
Lexer (Tokenization)
    ↓
Parser (AST Construction)
    ↓
Type Checker (Semantic Analysis)
    ↓
IR Generator (LLVM IR)
    ↓
Optimizer (Multiple Passes)
    ↓
Either:
  → Interpreter (Direct Execution or JIT)
  → Code Generator (Native Binary)
```

See [ARCHITECTURE.md](ARCHITECTURE.md) for detailed pipeline documentation.

### Standard Library Modules

| Module | Description | Documentation |
|--------|-------------|---------------|
| `math/*` | Mathematical operations | [STDLIB_GUIDE.md](STDLIB_GUIDE.md#math) |
| `data/*` | Data structures & algorithms | [STDLIB_GUIDE.md](STDLIB_GUIDE.md#data-structures--algorithms) |
| `web/*` | HTTP, WebSocket | [STDLIB_GUIDE.md](STDLIB_GUIDE.md#web--networking) |
| `database/*` | Database connectivity | [STDLIB_GUIDE.md](STDLIB_GUIDE.md#database) |
| `ml/*` | Machine learning | [STDLIB_GUIDE.md](STDLIB_GUIDE.md#machine-learning) |
| `game/*` | Game engine | [STDLIB_GUIDE.md](STDLIB_GUIDE.md#game-development) |
| `gui/*` | GUI framework | [STDLIB_GUIDE.md](STDLIB_GUIDE.md#gui) |
| `audio/*` | Audio processing | [STDLIB_GUIDE.md](STDLIB_GUIDE.md#audio) |
| `embedded/*` | GPIO, embedded systems | [STDLIB_GUIDE.md](STDLIB_GUIDE.md#embedded-systems) |
| `pkg/*` | Package management | [STDLIB_GUIDE.md](STDLIB_GUIDE.md#package-management) |
| `scripting/*` | Automation | [STDLIB_GUIDE.md](STDLIB_GUIDE.md#scripting--automation) |

### Build Configurations

| Configuration | Description | Use Case |
|---------------|-------------|----------|
| `-O0` | No optimization | Debugging |
| `-O1` | Basic optimization | Development |
| `-O2` | Standard optimization | Production |
| `-O3` | Aggressive optimization | Performance-critical |
| `--jit` | JIT compilation | Interactive/Testing |
| `--ast` | Print AST | Debugging |
| `--ir` | Print LLVM IR | Optimization analysis |

See [INTERPRETER_GUIDE.md](INTERPRETER_GUIDE.md#optimization-passes) for details.

## Additional Resources

### Examples
- **[../examples/](../examples/)** - Example programs demonstrating features
  - `concurrency_demo.to` - Goroutines and channels
  - `traits_demo.to` - Trait usage
  - `web_demo.to` - Web server example
  - `ml_demo.to` - Machine learning example
  - `game_demo.to` - Game development example

### Tests
- **[../tests/](../tests/)** - Test suite for compiler and runtime
- **[../test_interpreter_completion.py](../test_interpreter_completion.py)** - Interpreter tests

### Standard Library
- **[../stdlib/](../stdlib/)** - Complete standard library implementation

## Documentation Conventions

### Code Examples

Tocin code examples use the `.to` extension:

```to
// This is Tocin code
def greet(name: string) -> string {
    return f"Hello, {name}!";
}
```

C++ implementation examples are marked:

```cpp
// This is C++ implementation code
void IRGenerator::generate(ast::StmtPtr stmt) {
    stmt->accept(*this);
}
```

### Status Indicators

- ✅ Complete - Feature is implemented and tested
- 🔄 In Progress - Feature is being implemented
- 📋 Planned - Feature is planned but not started
- ⏳ Pending - Feature depends on other work

### File Structure

Documentation files follow this structure:
1. **Overview** - Brief description
2. **Table of Contents** - Navigation
3. **Main Content** - Detailed information
4. **Examples** - Code examples
5. **References** - Links to related docs

## Contributing to Documentation

To improve documentation:

1. **Fix Errors**: Submit PR with corrections
2. **Add Examples**: Provide clear, working examples
3. **Clarify**: Improve unclear sections
4. **Expand**: Add missing information
5. **Maintain**: Keep documentation in sync with code

### Documentation Style Guide

- Use **clear, concise language**
- Include **code examples** for concepts
- Provide **both simple and advanced examples**
- Link to **related documentation**
- Keep **consistent formatting**
- Update **index and TOC** when adding content

## Getting Help

- **Issues**: https://github.com/tafolabi009/tocin-compiler/issues
- **Discussions**: https://github.com/tafolabi009/tocin-compiler/discussions
- **Email**: dev@tocin.dev (if available)

## License

Documentation is licensed under the MIT License, same as the compiler.

---

**Last Updated**: 2024
**Compiler Version**: 1.0.0
**Documentation Version**: 1.0.0
