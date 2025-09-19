Tocin Compiler Installer Package
================================

This package contains a working Tocin Compiler with all required DLLs.

Contents:
- bin/tocin.exe - The main compiler executable
- bin/*.dll - All required dynamic libraries
- doc/ - Documentation files
- install.bat - Installation script (run as Administrator)
- uninstall.bat - Uninstallation script (run as Administrator)
- test_tocin.bat - Test script to verify installation

Installation:
1. Run install.bat as Administrator
2. The installer will copy files to Program Files and add to PATH
3. Run test_tocin.bat to verify the installation

Usage:
After installation, you can use 'tocin' from any command prompt:
- tocin --version
- tocin --help
- tocin compile myfile.to

Uninstallation:
Run uninstall.bat as Administrator to remove the compiler.

Note: This installer includes all required DLLs, so the compiler will work
on any Windows system without requiring additional dependencies.
