#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <memory>
#include <chrono>
#include "lexer/lexer.h"
#include "parser/parser.h"
#include "type/type_checker.h"
#include "lexer/token.h"
#include "codegen/ir_generator.h"
#include "compiler/compilation_context.h"
#include "error/error_handler.h"

// Print help and usage instructions.
void printUsage()
{
    std::cout << "Tocin Compiler v0.3.0\n";
    std::cout << "Usage:\n";
    std::cout << "  tocin-compiler [options] <file.to>\n";
    std::cout << "Options:\n";
    std::cout << "  -h, --help        Show this help message.\n";
    std::cout << "  -O <level>        Set optimization level (default: 0, max: 3).\n";
    std::cout << "  -g                Enable debug information.\n";
    std::cout << "  -o <output file>  Specify output file for generated LLVM IR.\n";
    std::cout << "  -r                Enter REPL mode.\n";
    std::cout << "\nExamples:\n";
    std::cout << "  tocin-compiler myprogram.to\n";
    std::cout << "  tocin-compiler -O 2 -g -o output.ll myprogram.to\n";
    std::cout << "  tocin-compiler -r (Launch interactive REPL mode)\n";
}

// REPL Mode (Interactive Interpreter)
void launchREPL()
{
    std::cout << "Tocin REPL (type 'exit' to quit)\n";
    std::string input;

    while (true)
    {
        std::cout << ">>> ";
        std::getline(std::cin, input);

        if (input == "exit") break;

        try
        {
            Lexer lexer(input, "<stdin>");
            auto tokens = lexer.tokenize();
            auto parser = std::make_unique<Parser>(tokens);
            auto ast = parser->parse();

            TypeChecker typeChecker;
            typeChecker.check(ast);

            IRGenerator irGen;
            irGen.generate(ast, "");
        }
        catch (const std::exception& e)
        {
            std::cerr << "Error: " << e.what() << std::endl;
        }
    }
}

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        printUsage();
        return 1;
    }

    std::string inputFile;
    std::string outputFile;
    int optimizationLevel = 0;
    bool debug = false;
    bool useREPL = false;

    // Parse command-line arguments.
    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help")
        {
            printUsage();
            return 0;
        }
        else if (arg == "-O")
        {
            if (i + 1 < argc)
            {
                try {
                    optimizationLevel = std::stoi(argv[++i]);
                    if (optimizationLevel < 0 || optimizationLevel > 3)
                    {
                        std::cerr << "Warning: Invalid optimization level. Using default (0).\n";
                        optimizationLevel = 0;
                    }
                }
                catch (...) {
                    std::cerr << "Error: Invalid optimization level provided.\n";
                    return 1;
                }
            }
            else
            {
                std::cerr << "Error: Missing optimization level after -O.\n";
                return 1;
            }
        }
        else if (arg == "-g")
        {
            debug = true;
        }
        else if (arg == "-o")
        {
            if (i + 1 < argc)
            {
                outputFile = argv[++i];
            }
            else
            {
                std::cerr << "Error: Missing output file after -o.\n";
                return 1;
            }
        }
        else if (arg == "-r")
        {
            useREPL = true;
        }
        else if (arg[0] == '-')
        {
            std::cerr << "Unknown option: " << arg << "\n";
            return 1;
        }
        else
        {
            inputFile = arg;
        }
    }

    // Launch REPL if requested
    if (useREPL)
    {
        launchREPL();
        return 0;
    }

    if (inputFile.empty())
    {
        std::cerr << "Error: No input file provided.\n";
        return 1;
    }

    // Read source code from the input file.
    std::ifstream file(inputFile);
    if (!file)
    {
        std::cerr << "Error: Could not open file " << inputFile << "\n";
        return 1;
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string sourceCode = buffer.str();

    // Remove Shebang if present
    if (sourceCode.starts_with("#!"))
    {
        size_t newline = sourceCode.find('\n');
        if (newline != std::string::npos)
        {
            sourceCode = sourceCode.substr(newline + 1);
        }
    }

    // Measure compilation time
    auto startTime = std::chrono::high_resolution_clock::now();

    try
    {
        std::cout << "Starting compilation...\n";
        Lexer lexer(sourceCode, inputFile);
        std::cout << "Lexing completed.\n";

        auto tokens = lexer.tokenize();
        std::cout << "Tokenization completed.\n";

        auto parser = std::make_unique<Parser>(tokens);
        std::cout << "Parsing started...\n";

        auto ast = parser->parse();
        std::cout << "Parsing completed.\n";

        std::cout << "Type checking started...\n";
        TypeChecker typeChecker;
        typeChecker.check(ast);
        std::cout << "Type checking completed.\n";

        std::cout << "IR Generation started...\n";
        IRGenerator irGen;
        irGen.generate(ast, outputFile);
        std::cout << "IR Generation completed.\n";
    }
    catch (const std::exception& e)
    {
        std::cerr << "Compilation Error: " << e.what() << std::endl;
        return 1;
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = endTime - startTime;
    std::cout << "Compilation completed in " << elapsed.count() << " seconds.\n";

    return 0;
}
