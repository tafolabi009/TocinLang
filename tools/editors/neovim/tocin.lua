local M = {}

-- LSP setup
function M.setup()
    local lspconfig = require('lspconfig')
    local configs = require('lspconfig.configs')

    -- Register custom language server if not already registered
    if not configs.tocin_ls then
        configs.tocin_ls = {
            default_config = {
                cmd = {'node', vim.fn.expand('$HOME/.local/share/tocin/lsp/out/server.js'), '--stdio'},
                filetypes = {'tocin'},
                root_dir = lspconfig.util.root_pattern('.git', 'tocin.toml'),
                settings = {},
                init_options = {
                    formatting = true,
                    diagnostics = true,
                    semanticTokens = true,
                    codeActions = true
                }
            }
        }
    end

    -- Configure LSP client
    lspconfig.tocin_ls.setup {
        capabilities = require('cmp_nvim_lsp').default_capabilities(),
        on_attach = function(client, bufnr)
            -- Enable completion triggered by <c-x><c-o>
            vim.api.nvim_buf_set_option(bufnr, 'omnifunc', 'v:lua.vim.lsp.omnifunc')

            -- Mappings
            local bufopts = { noremap=true, silent=true, buffer=bufnr }
            vim.keymap.set('n', 'gD', vim.lsp.buf.declaration, bufopts)
            vim.keymap.set('n', 'gd', vim.lsp.buf.definition, bufopts)
            vim.keymap.set('n', 'K', vim.lsp.buf.hover, bufopts)
            vim.keymap.set('n', 'gi', vim.lsp.buf.implementation, bufopts)
            vim.keymap.set('n', '<C-k>', vim.lsp.buf.signature_help, bufopts)
            vim.keymap.set('n', '<space>wa', vim.lsp.buf.add_workspace_folder, bufopts)
            vim.keymap.set('n', '<space>wr', vim.lsp.buf.remove_workspace_folder, bufopts)
            vim.keymap.set('n', '<space>D', vim.lsp.buf.type_definition, bufopts)
            vim.keymap.set('n', '<space>rn', vim.lsp.buf.rename, bufopts)
            vim.keymap.set('n', '<space>ca', vim.lsp.buf.code_action, bufopts)
            vim.keymap.set('n', 'gr', vim.lsp.buf.references, bufopts)
            vim.keymap.set('n', '<space>f', function() vim.lsp.buf.format { async = true } end, bufopts)
        end
    }

    -- Set up treesitter for syntax highlighting
    local parser_config = require('nvim-treesitter.parsers').get_parser_configs()
    parser_config.tocin = {
        install_info = {
            url = 'https://github.com/tocin-lang/tree-sitter-tocin',
            files = {'src/parser.c'},
            branch = 'main'
        },
        filetype = 'tocin'
    }
end

-- File type detection
vim.filetype.add({
    extension = {
        to = 'tocin'
    }
})

-- Commands
vim.api.nvim_create_user_command('TocinFormat', function()
    vim.lsp.buf.format({ async = true })
end, {})

vim.api.nvim_create_user_command('TocinRestartLSP', function()
    vim.lsp.stop_client(vim.lsp.get_active_clients())
    vim.cmd('edit')
end, {})

return M 