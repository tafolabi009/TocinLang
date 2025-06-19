import * as path from 'path';
import * as vscode from 'vscode';
import {
    LanguageClient,
    LanguageClientOptions,
    ServerOptions,
    TransportKind
} from 'vscode-languageclient/node';

let client: LanguageClient;

export async function activate(context: vscode.ExtensionContext) {
    // Server module
    const serverModule = context.asAbsolutePath(path.join('..', 'lsp', 'out', 'server.js'));
    
    // Server options - debug vs. run mode
    const serverOptions: ServerOptions = {
        run: {
            module: serverModule,
            transport: TransportKind.ipc
        },
        debug: {
            module: serverModule,
            transport: TransportKind.ipc,
            options: { execArgv: ['--nolazy', '--inspect=6009'] }
        }
    };

    // Client options - document selector etc.
    const clientOptions: LanguageClientOptions = {
        documentSelector: [{ scheme: 'file', language: 'tocin' }],
        synchronize: {
            fileEvents: vscode.workspace.createFileSystemWatcher('**/*.to')
        },
        middleware: {
            // Add middleware for advanced features
            provideDocumentFormattingEdits: async (document, options, token, next) => {
                // Custom formatting logic if needed
                return next(document, options, token);
            }
        }
    };

    // Create and start the client
    client = new LanguageClient(
        'tocinLanguageServer',
        'Tocin Language Server',
        serverOptions,
        clientOptions
    );

    // Register semantic tokens provider
    const tokenTypes = [
        'namespace', 'type', 'class', 'enum', 'interface',
        'struct', 'typeParameter', 'parameter', 'variable',
        'property', 'enumMember', 'event', 'function',
        'method', 'macro', 'keyword', 'modifier', 'comment',
        'string', 'number', 'regexp', 'operator'
    ];
    const tokenModifiers = [
        'declaration', 'definition', 'readonly', 'static',
        'deprecated', 'abstract', 'async', 'modification',
        'documentation', 'defaultLibrary'
    ];

    const legend = new vscode.SemanticTokensLegend(tokenTypes, tokenModifiers);

    context.subscriptions.push(
        vscode.languages.registerDocumentSemanticTokensProvider(
            { language: 'tocin' },
            client.getFeature('textDocument/semanticTokens'),
            legend
        )
    );

    // Register code actions provider
    context.subscriptions.push(
        vscode.languages.registerCodeActionsProvider(
            'tocin',
            {
                provideCodeActions: (document, range, context, token) => {
                    const actions: vscode.CodeAction[] = [];
                    // Add code actions based on diagnostics
                    for (const diagnostic of context.diagnostics) {
                        if (diagnostic.code === 'unused-import') {
                            const action = new vscode.CodeAction(
                                'Remove unused import',
                                vscode.CodeActionKind.QuickFix
                            );
                            action.edit = new vscode.WorkspaceEdit();
                            action.edit.delete(document.uri, diagnostic.range);
                            actions.push(action);
                        }
                    }
                    return actions;
                }
            },
            {
                providedCodeActionKinds: [
                    vscode.CodeActionKind.QuickFix,
                    vscode.CodeActionKind.Refactor,
                    vscode.CodeActionKind.RefactorExtract,
                    vscode.CodeActionKind.RefactorInline
                ]
            }
        )
    );

    // Register commands
    context.subscriptions.push(
        vscode.commands.registerCommand('tocin.restart', async () => {
            await client.stop();
            await client.start();
        }),
        vscode.commands.registerCommand('tocin.format', async () => {
            const editor = vscode.window.activeTextEditor;
            if (editor) {
                await vscode.commands.executeCommand('editor.action.formatDocument');
            }
        })
    );

    // Start the client
    await client.start();
}

export function deactivate(): Thenable<void> | undefined {
    if (!client) {
        return undefined;
    }
    return client.stop();
} 