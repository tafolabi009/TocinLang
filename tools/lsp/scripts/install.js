const fs = require('fs');
const path = require('path');
const os = require('os');
const { execSync } = require('child_process');

// Configuration
const config = {
    lspInstallDir: path.join(os.homedir(), '.local', 'share', 'tocin', 'lsp'),
    editors: {
        vscode: {
            installDir: process.platform === 'win32'
                ? path.join(os.homedir(), 'AppData', 'Local', 'Programs', 'Microsoft VS Code', 'resources', 'app', 'extensions')
                : process.platform === 'darwin'
                    ? path.join(os.homedir(), 'Library', 'Application Support', 'Code', 'User', 'extensions')
                    : path.join(os.homedir(), '.vscode', 'extensions'),
            name: 'tocin-vscode'
        },
        sublime: {
            installDir: process.platform === 'win32'
                ? path.join(os.homedir(), 'AppData', 'Roaming', 'Sublime Text', 'Packages')
                : process.platform === 'darwin'
                    ? path.join(os.homedir(), 'Library', 'Application Support', 'Sublime Text', 'Packages')
                    : path.join(os.homedir(), '.config', 'sublime-text', 'Packages'),
            name: 'Tocin'
        },
        neovim: {
            installDir: path.join(os.homedir(), '.local', 'share', 'nvim', 'site', 'pack', 'tocin', 'start'),
            name: 'tocin.nvim'
        }
    }
};

// Ensure directory exists
function ensureDir(dir) {
    if (!fs.existsSync(dir)) {
        fs.mkdirSync(dir, { recursive: true });
    }
}

// Install LSP server
async function installLSPServer() {
    console.log('Installing LSP server...');
    ensureDir(config.lspInstallDir);

    // Copy LSP server files
    const lspSourceDir = path.join(__dirname, '..', 'out');
    fs.cpSync(lspSourceDir, config.lspInstallDir, { recursive: true });

    // Install dependencies
    console.log('Installing LSP server dependencies...');
    execSync('npm install --production', { cwd: config.lspInstallDir, stdio: 'inherit' });
}

// Install editor extensions
async function installEditorExtensions() {
    console.log('Installing editor extensions...');

    // VSCode
    if (fs.existsSync(config.editors.vscode.installDir)) {
        console.log('Installing VSCode extension...');
        const vscodeExtDir = path.join(config.editors.vscode.installDir, config.editors.vscode.name);
        ensureDir(vscodeExtDir);
        fs.cpSync(path.join(__dirname, '../../vscode'), vscodeExtDir, { recursive: true });
    }

    // Sublime Text
    if (fs.existsSync(config.editors.sublime.installDir)) {
        console.log('Installing Sublime Text package...');
        const sublimePackageDir = path.join(config.editors.sublime.installDir, config.editors.sublime.name);
        ensureDir(sublimePackageDir);
        fs.cpSync(path.join(__dirname, '../../editors/sublime/Tocin.sublime-package'), sublimePackageDir, { recursive: true });
    }

    // Neovim
    if (fs.existsSync(path.dirname(config.editors.neovim.installDir))) {
        console.log('Installing Neovim plugin...');
        const neovimPluginDir = path.join(config.editors.neovim.installDir, config.editors.neovim.name);
        ensureDir(neovimPluginDir);
        fs.cpSync(path.join(__dirname, '../../editors/neovim'), neovimPluginDir, { recursive: true });
    }
}

// Main installation process
async function main() {
    try {
        await installLSPServer();
        await installEditorExtensions();
        console.log('Installation completed successfully!');
    } catch (error) {
        console.error('Installation failed:', error);
        process.exit(1);
    }
}

main();