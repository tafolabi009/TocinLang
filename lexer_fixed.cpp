// Fixed lexer.cpp - Key changes made:

// 1. Fixed skipWhitespace() to properly handle newlines
void Lexer::skipWhitespace()
{
    while (true)
    {
        char c = peek();
        if (c == ' ' || c == '\r' || c == '\t')
        {
            advance();
        }
        else if (c == '\n')
        {
            ++line;
            column = 1;
            atLineStart = true;
            advance();
            // Don't break here - continue to handle indentation on next line
            continue;
        }
        else if (c == '#')
        {
            // Handle both single-line and multi-line comments
            advance(); // consume '#'
            if (match('#')) // ## for multi-line comments
            {
                while (!isAtEnd() && !(peek() == '#' && peekNext() == '#'))
                {
                    if (peek() == '\n')
                    {
                        ++line;
                        column = 1;
                        atLineStart = true;
                    }
                    advance();
                }
                if (match('#')) advance(); // consume closing ##
            }
            else // Single-line comment
            {
                while (!isAtEnd() && peek() != '\n')
                    advance();
            }
        }
        else
        {
            break;
        }
    }
}

// 2. Fixed scanToken() to properly handle indentation
void Lexer::scanToken()
{
    if (atLineStart) {
        handleIndentation();
        atLineStart = false;
        // Skip any remaining whitespace after indentation
        while (peek() == ' ' || peek() == '\r' || peek() == '\t') {
            advance();
        }
    } else {
        skipWhitespace();
    }
    start = current;
    if (isAtEnd())
        return;

    char c = peek();  // Don't advance yet, just peek
    if (std::isalpha(c) || c == '_')
    {
        scanIdentifier();
        return;
    }
    if (std::isdigit(c))
    {
        scanNumber();
        return;
    }

    // For all other tokens, advance and process
    advance();  // Now advance for non-identifier tokens

    switch (c)
    {
    // ... rest of the switch statement remains the same
    }
}

// 3. The handleIndentation() function is correct as-is
// 4. All other functions (scanString, scanNumber, scanIdentifier) are correct
