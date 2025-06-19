import {
    createConnection,
    TextDocuments,
    Diagnostic,
    DiagnosticSeverity,
    ProposedFeatures,
    InitializeParams,
    TextDocumentSyncKind,
    InitializeResult,
    CodeActionKind,
    DocumentFormattingParams,
    TextEdit,
    Range,
    Position,
    SemanticTokensBuilder,
    SemanticTokensLegend
} from 'vscode-languageserver/node';

import { TextDocument } from 'vscode-languageserver-textdocument';

// Create a connection for the server
const connection = createConnection(ProposedFeatures.all);

// Create a text document manager
const documents: TextDocuments<TextDocument> = new TextDocuments(TextDocument);

let hasConfigurationCapability = false;
let hasWorkspaceFolderCapability = false;
let hasDiagnosticRelatedInformationCapability = false;

// Initialize the server
connection.onInitialize((params: InitializeParams) => {
    const capabilities = params.capabilities;

    hasConfigurationCapability = !!(
        capabilities.workspace && !!capabilities.workspace.configuration
    );
    hasWorkspaceFolderCapability = !!(
        capabilities.workspace && !!capabilities.workspace.workspaceFolders
    );
    hasDiagnosticRelatedInformationCapability = !!(
        capabilities.textDocument &&
        capabilities.textDocument.publishDiagnostics &&
        capabilities.textDocument.publishDiagnostics.relatedInformation
    );

    const result: InitializeResult = {
        capabilities: {
            textDocumentSync: TextDocumentSyncKind.Incremental,
            completionProvider: {
                resolveProvider: true,
                triggerCharacters: ['.', ':', '<']
            },
            hoverProvider: true,
            documentFormattingProvider: true,
            documentSymbolProvider: true,
            definitionProvider: true,
            referencesProvider: true,
            codeActionProvider: {
                codeActionKinds: [
                    CodeActionKind.QuickFix,
                    CodeActionKind.Refactor
                ]
            },
            semanticTokensProvider: {
                legend: {
                    tokenTypes: [
                        'namespace', 'type', 'class', 'enum', 'interface',
                        'struct', 'typeParameter', 'parameter', 'variable',
                        'property', 'enumMember', 'event', 'function',
                        'method', 'macro', 'keyword', 'modifier', 'comment',
                        'string', 'number', 'regexp', 'operator'
                    ],
                    tokenModifiers: [
                        'declaration', 'definition', 'readonly', 'static',
                        'deprecated', 'abstract', 'async', 'modification',
                        'documentation', 'defaultLibrary'
                    ]
                },
                full: true,
                range: false
            }
        }
    };

    return result;
});

// Initialize workspace
connection.onInitialized(() => {
    if (hasConfigurationCapability) {
        connection.client.register(
            'workspace/didChangeConfiguration',
            undefined
        );
    }
    if (hasWorkspaceFolderCapability) {
        connection.workspace.onDidChangeWorkspaceFolders(_event => {
            connection.console.log('Workspace folder change event received.');
        });
    }
});

// Document formatting
connection.onDocumentFormatting(
    (params: DocumentFormattingParams): TextEdit[] => {
        const document = documents.get(params.textDocument.uri);
        if (!document) {
            return [];
        }

        // Simple formatting example - indent with 4 spaces
        const text = document.getText();
        const lines = text.split(/\r?\n/);
        const edits: TextEdit[] = [];

        let indentLevel = 0;
        lines.forEach((line, i) => {
            const trimmed = line.trim();
            if (trimmed.endsWith('{')) {
                const indent = ' '.repeat(indentLevel * 4);
                edits.push(TextEdit.replace(
                    Range.create(Position.create(i, 0), Position.create(i, line.length)),
                    indent + trimmed
                ));
                indentLevel++;
            } else if (trimmed.startsWith('}')) {
                indentLevel = Math.max(0, indentLevel - 1);
                const indent = ' '.repeat(indentLevel * 4);
                edits.push(TextEdit.replace(
                    Range.create(Position.create(i, 0), Position.create(i, line.length)),
                    indent + trimmed
                ));
            } else if (trimmed.length > 0) {
                const indent = ' '.repeat(indentLevel * 4);
                edits.push(TextEdit.replace(
                    Range.create(Position.create(i, 0), Position.create(i, line.length)),
                    indent + trimmed
                ));
            }
        });

        return edits;
    }
);

// Semantic tokens
connection.languages.semanticTokens.on(params => {
    const document = documents.get(params.textDocument.uri);
    if (!document) {
        return {
            data: []
        };
    }

    const tokensBuilder = new SemanticTokensBuilder();
    const text = document.getText();
    
    // Simple example - highlight keywords
    const keywords = ['fn', 'let', 'if', 'else', 'while', 'for', 'return'];
    const keywordRegex = new RegExp(`\\b(${keywords.join('|')})\\b`, 'g');
    
    let match;
    while ((match = keywordRegex.exec(text)) !== null) {
        const startPos = document.positionAt(match.index);
        tokensBuilder.push(
            startPos.line,
            startPos.character,
            match[0].length,
            15, // keyword token type
            0   // no modifiers
        );
    }

    return tokensBuilder.build();
});

// Listen for text document changes
documents.onDidChangeContent(change => {
    validateTextDocument(change.document);
});

// Validate a text document
async function validateTextDocument(textDocument: TextDocument): Promise<void> {
    const text = textDocument.getText();
    const diagnostics: Diagnostic[] = [];

    // Simple validation example - check for TODO comments
    const todoPattern = /TODO:/g;
    let match;

    while ((match = todoPattern.exec(text))) {
        const diagnostic: Diagnostic = {
            severity: DiagnosticSeverity.Information,
            range: {
                start: textDocument.positionAt(match.index),
                end: textDocument.positionAt(match.index + match[0].length)
            },
            message: 'TODO comment found',
            source: 'tocin'
        };

        if (hasDiagnosticRelatedInformationCapability) {
            diagnostic.relatedInformation = [
                {
                    location: {
                        uri: textDocument.uri,
                        range: Object.assign({}, diagnostic.range)
                    },
                    message: 'Consider resolving this TODO'
                }
            ];
        }

        diagnostics.push(diagnostic);
    }

    connection.sendDiagnostics({ uri: textDocument.uri, diagnostics });
}

// Make the text document manager listen on the connection
documents.listen(connection);

// Listen on the connection
connection.listen(); 