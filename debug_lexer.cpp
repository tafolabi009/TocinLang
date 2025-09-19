#include <iostream>
#include <string>
#include "src/lexer/lexer.h"
#include "src/lexer/token.h"

int main() {
    std::string source = "def main() -> int {\n    return 0;\n}";
    std::cout << "Source: " << source << std::endl;
    
    lexer::Lexer lexer(source, "test", 4);
    std::vector<lexer::Token> tokens = lexer.tokenize();
    
    std::cout << "Tokens:" << std::endl;
    for (const auto& token : tokens) {
        std::cout << "  " << token.toString() << std::endl;
    }
    
    return 0;
}
