import { TextDocument, Position, Range } from 'vscode-languageserver-textdocument';

export interface TokenType {
    name: string;
    scope: string;
    pattern: RegExp;
}

// Define token types for Tocin
export const tokenTypes: TokenType[] = [
    {
        name: 'keyword',
        scope: 'keyword.control.tocin',
        pattern: /\b(fn|let|const|if|else|while|for|return|struct|trait|impl|async|await|match|go|channel|select|defer|import|export)\b/
    },
    {
        name: 'type',
        scope: 'support.type.tocin',
        pattern: /\b(int|float|bool|string|void|any|Option|Result|Array|Map|Set)\b/
    },
    {
        name: 'string',
        scope: 'string.quoted.double.tocin',
        pattern: /"(?:[^"\\]|\\.)*"/
    },
    {
        name: 'number',
        scope: 'constant.numeric.tocin',
        pattern: /\b\d+(\.\d+)?([eE][+-]?\d+)?\b/
    },
    {
        name: 'comment',
        scope: 'comment.line.double-slash.tocin',
        pattern: /\/\/.*$/
    },
    {
        name: 'multilineComment',
        scope: 'comment.block.tocin',
        pattern: /\/\*[\s\S]*?\*\//
    },
    {
        name: 'function',
        scope: 'entity.name.function.tocin',
        pattern: /\b[a-zA-Z_]\w*(?=\s*\()/
    },
    {
        name: 'variable',
        scope: 'variable.other.tocin',
        pattern: /\b[a-zA-Z_]\w*\b/
    },
    {
        name: 'operator',
        scope: 'keyword.operator.tocin',
        pattern: /[+\-*/%=<>!&|^~?:]+/
    }
];

// Theme settings for syntax highlighting
export const defaultTheme = {
    keyword: { foreground: '#0000ff', fontStyle: 'bold' },
    type: { foreground: '#008080' },
    string: { foreground: '#a31515' },
    number: { foreground: '#098658' },
    comment: { foreground: '#008000', fontStyle: 'italic' },
    function: { foreground: '#795E26' },
    variable: { foreground: '#001080' },
    operator: { foreground: '#000000' }
};

export interface Token {
    type: TokenType;
    range: Range;
    text: string;
}

export function tokenize(document: TextDocument): Token[] {
    const text = document.getText();
    const tokens: Token[] = [];
    const lines = text.split('\n');

    for (let lineNum = 0; lineNum < lines.length; lineNum++) {
        const line = lines[lineNum];
        
        for (const tokenType of tokenTypes) {
            let match;
            while ((match = tokenType.pattern.exec(line)) !== null) {
                const startPos = Position.create(lineNum, match.index);
                const endPos = Position.create(lineNum, match.index + match[0].length);
                
                tokens.push({
                    type: tokenType,
                    range: Range.create(startPos, endPos),
                    text: match[0]
                });
            }
        }
    }

    return tokens;
}

export function getSemanticTokens(document: TextDocument): number[] {
    const tokens = tokenize(document);
    const builder: number[] = [];
    let prevLine = 0;
    let prevChar = 0;

    for (const token of tokens) {
        const deltaLine = token.range.start.line - prevLine;
        const deltaStart = deltaLine === 0 
            ? token.range.start.character - prevChar 
            : token.range.start.character;

        builder.push(
            deltaLine,
            deltaStart,
            token.range.end.character - token.range.start.character,
            getTokenTypeIndex(token.type),
            0 // modifiers
        );

        prevLine = token.range.start.line;
        prevChar = token.range.start.character;
    }

    return builder;
}

function getTokenTypeIndex(type: TokenType): number {
    return tokenTypes.indexOf(type);
} 