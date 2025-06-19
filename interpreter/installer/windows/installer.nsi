!include "MUI2.nsh"
!include "FileFunc.nsh"

Name "Tocin"
OutFile "tocin-setup.exe"
InstallDir "$PROGRAMFILES64\Tocin"
RequestExecutionLevel admin

!define MUI_ABORTWARNING
!define MUI_ICON "${NSISDIR}\Contrib\Graphics\Icons\modern-install.ico"
!define MUI_UNICON "${NSISDIR}\Contrib\Graphics\Icons\modern-uninstall.ico"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "LICENSE"
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"

Section "Install"
    SetOutPath "$INSTDIR"
    
    # Copy executables
    File "bin\tocin.exe"
    File "bin\tocin-repl.exe"
    
    # Copy DLLs
    File "bin\*.dll"
    
    # Copy documentation
    SetOutPath "$INSTDIR\docs"
    File /r "docs\*.*"
    
    # Create start menu shortcuts
    CreateDirectory "$SMPROGRAMS\Tocin"
    CreateShortcut "$SMPROGRAMS\Tocin\Tocin.lnk" "$INSTDIR\tocin.exe"
    CreateShortcut "$SMPROGRAMS\Tocin\Tocin REPL.lnk" "$INSTDIR\tocin-repl.exe"
    CreateShortcut "$SMPROGRAMS\Tocin\Uninstall.lnk" "$INSTDIR\uninstall.exe"
    
    # Add to PATH
    EnVar::AddValue "PATH" "$INSTDIR"
    
    # Write uninstaller
    WriteUninstaller "$INSTDIR\uninstall.exe"
    
    # Write registry keys
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Tocin" \
                     "DisplayName" "Tocin"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Tocin" \
                     "UninstallString" "$\"$INSTDIR\uninstall.exe$\""
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Tocin" \
                     "DisplayIcon" "$INSTDIR\tocin.exe"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Tocin" \
                     "Publisher" "Your Organization"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Tocin" \
                     "URLInfoAbout" "https://github.com/yourusername/tocin"
SectionEnd

Section "Uninstall"
    # Remove files
    Delete "$INSTDIR\tocin.exe"
    Delete "$INSTDIR\tocin-repl.exe"
    Delete "$INSTDIR\*.dll"
    RMDir /r "$INSTDIR\docs"
    
    # Remove shortcuts
    Delete "$SMPROGRAMS\Tocin\Tocin.lnk"
    Delete "$SMPROGRAMS\Tocin\Tocin REPL.lnk"
    Delete "$SMPROGRAMS\Tocin\Uninstall.lnk"
    RMDir "$SMPROGRAMS\Tocin"
    
    # Remove from PATH
    EnVar::DeleteValue "PATH" "$INSTDIR"
    
    # Remove registry keys
    DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Tocin"
    
    # Remove installation directory
    RMDir "$INSTDIR"
SectionEnd 