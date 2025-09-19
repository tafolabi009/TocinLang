; Tocin Compiler Windows Installer Script

!define PRODUCT_NAME "Tocin Compiler"
!define VERSION "1.0.0"
!define PUBLISHER "Tocin Team"
!define WEBSITE "https://tocin.dev"

; Modern UI
!include "MUI2.nsh"

; Installer settings
Name "${PRODUCT_NAME}"
OutFile "TocingCompiler-${VERSION}-Setup.exe"
InstallDir "$PROGRAMFILES64\Tocin"
InstallDirRegKey HKLM "Software\Tocin" "Install_Dir"
RequestExecutionLevel admin

; MUI Settings
!define MUI_ABORTWARNING
!define MUI_ICON "..\common\assets\icon.ico"
!define MUI_UNICON "..\common\assets\icon.ico"
!define MUI_WELCOMEFINISHPAGE_BITMAP "..\common\assets\installer_banner.bmp"
!define MUI_UNWELCOMEFINISHPAGE_BITMAP "..\common\assets\installer_banner.bmp"

; Pages
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "..\..\LICENSE"
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

; Uninstaller pages
!insertmacro MUI_UNPAGE_WELCOME
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_UNPAGE_FINISH

; Language
!insertmacro MUI_LANGUAGE "English"

; Installer sections
Section "Tocin Compiler" SecMain
    SectionIn RO
    SetOutPath "$INSTDIR"
    
    ; Install files
    File /r "..\..\build\installer_staging\bin\*"
    File /r "..\..\build\installer_staging\lib\*"
    File /r "..\..\build\installer_staging\doc\*"
    
    ; Create uninstaller
    WriteUninstaller "$INSTDIR\uninstall.exe"
    
    ; Create shortcuts
    CreateDirectory "$SMPROGRAMS\Tocin"
    CreateShortcut "$SMPROGRAMS\Tocin\Tocin Compiler.lnk" "$INSTDIR\bin\tocin.exe"
    CreateShortcut "$SMPROGRAMS\Tocin\Documentation.lnk" "$INSTDIR\doc\index.html"
    CreateShortcut "$SMPROGRAMS\Tocin\Uninstall.lnk" "$INSTDIR\uninstall.exe"
    
    ; Write registry keys
    WriteRegStr HKLM "Software\Tocin" "Install_Dir" "$INSTDIR"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Tocin" "DisplayName" "${PRODUCT_NAME}"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Tocin" "UninstallString" '"$INSTDIR\uninstall.exe"'
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Tocin" "DisplayIcon" "$INSTDIR\bin\tocin.exe"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Tocin" "DisplayVersion" "${VERSION}"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Tocin" "Publisher" "${PUBLISHER}"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Tocin" "URLInfoAbout" "${WEBSITE}"
    
    ; Add to PATH
    EnVar::SetHKLM
    EnVar::AddValue "Path" "$INSTDIR\bin"
SectionEnd

; Uninstaller section
Section "Uninstall"
    ; Remove files
    RMDir /r "$INSTDIR\bin"
    RMDir /r "$INSTDIR\lib"
    RMDir /r "$INSTDIR\doc"
    Delete "$INSTDIR\uninstall.exe"
    RMDir "$INSTDIR"
    
    ; Remove shortcuts
    Delete "$SMPROGRAMS\Tocin\*.*"
    RMDir "$SMPROGRAMS\Tocin"
    
    ; Remove registry keys
    DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Tocin"
    DeleteRegKey HKLM "Software\Tocin"
    
    ; Remove from PATH
    EnVar::SetHKLM
    EnVar::DeleteValue "Path" "$INSTDIR\bin"
SectionEnd 