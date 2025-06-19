const fs = require('fs');
const path = require('path');
const { execSync } = require('child_process');

// Configuration
const config = {
    outDir: 'out',
    distDir: 'dist',
    packageName: 'tocin-language-server'
};

// Ensure directories exist
function ensureDir(dir) {
    if (!fs.existsSync(dir)) {
        fs.mkdirSync(dir, { recursive: true });
    }
}

// Clean directories
function clean() {
    console.log('Cleaning output directories...');
    [config.outDir, config.distDir].forEach(dir => {
        if (fs.existsSync(dir)) {
            fs.rmSync(dir, { recursive: true, force: true });
        }
        ensureDir(dir);
    });
}

// Build TypeScript
function buildTypeScript() {
    console.log('Building TypeScript...');
    execSync('tsc -p .', { stdio: 'inherit' });
}

// Package for distribution
function package() {
    console.log('Packaging for distribution...');
    
    // Copy necessary files
    const filesToCopy = [
        'package.json',
        'README.md',
        'LICENSE'
    ];

    filesToCopy.forEach(file => {
        if (fs.existsSync(file)) {
            fs.copyFileSync(file, path.join(config.outDir, file));
        }
    });

    // Create distribution package
    const distPath = path.join(config.distDir, `${config.packageName}.zip`);
    
    if (process.platform === 'win32') {
        execSync(`powershell Compress-Archive -Path ${config.outDir}/* -DestinationPath ${distPath} -Force`);
    } else {
        execSync(`cd ${config.outDir} && zip -r ../${distPath} ./*`);
    }
}

// Main build process
function main() {
    try {
        clean();
        buildTypeScript();
        package();
        console.log('Build completed successfully!');
    } catch (error) {
        console.error('Build failed:', error);
        process.exit(1);
    }
}

main(); 