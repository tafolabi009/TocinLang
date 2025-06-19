#include "Interpreter.h"
#include "Runtime.h"
#include "Builtins.h"
#include "../tocin-compiler/src/lexer/lexer.h"
#include "../tocin-compiler/src/parser/parser.h"
#include "../tocin-compiler/src/error/error_handler.h"
#include <iostream>
#include <string>
#include <unordered_map>

namespace interpreter {

    void runRepl() {
        error::ErrorHandler errorHandler;
        Interpreter interpreter(errorHandler);
        std::unordered_map<std::string, BuiltinFunction> builtins;
        Builtins::registerBuiltins(builtins);

        std::cout << "Tocin Interpreter REPL" << std::endl;
        std::cout << "Type 'exit' to quit" << std::endl;

        while (true) {
            std::cout << "> ";
            std::string input;
            std::getline(std::cin, input);

            if (input == "exit") {
                break;
            }

            lexer::Lexer lexer(input, "<repl>");
            std::vector<lexer::Token> tokens = lexer.tokenize();

            if (errorHandler.hasErrors()) {
                errorHandler.clearErrors();
                continue;
            }

            parser::Parser parser(tokens);
            ast::StmtPtr stmt = parser.parse();

            if (errorHandler.hasErrors()) {
                errorHandler.clearErrors();
                continue;
            }

            interpreter.interpret(stmt);
        }
    }

} // namespace interpreter

int main() {
    interpreter::runRepl();
    return 0;
} 