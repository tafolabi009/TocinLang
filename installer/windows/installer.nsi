; Tocin Compiler — Windows installer (NSIS / Modern UI 2)
;
; Produces a GUI Setup.exe: welcome -> license -> components -> directory ->
; install -> finish, plus a matching uninstaller. Adds Tocin to PATH, registers
; a .to file association, and installs the self-contained toolchain (compiler +
; bundled ld.lld link bundle) so native `-o` output builds with NO system
; compiler. PATH editing uses pure NSIS + WriteRegExpandStr (no external EnVar
; plugin), so this compiles and runs on a stock makensis.
;
; Build (on Windows, after staging files into ..\..\dist\staging\):
;   makensis /DVERSION=0.1.0 installer.nsi
; STAGE can be overridden: makensis /DSTAGE=path installer.nsi

!ifndef VERSION
  !define VERSION "0.1.0"
!endif
!ifndef STAGE
  !define STAGE "..\..\dist\staging"
!endif
!define PRODUCT_NAME "Tocin"
!define PUBLISHER "Afolabi Oluwatosin"
!define WEBSITE "https://github.com/tafolabi009/TocinLang"
!define UNINST_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\Tocin"

!include "MUI2.nsh"
!include "LogicLib.nsh"
!include "WinMessages.nsh"

Name "${PRODUCT_NAME} ${VERSION}"
OutFile "Tocin-${VERSION}-Setup.exe"
InstallDir "$PROGRAMFILES64\Tocin"
InstallDirRegKey HKLM "Software\Tocin" "Install_Dir"
RequestExecutionLevel admin
Unicode true
SetCompressor /SOLID lzma

!define MUI_ABORTWARNING
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "..\..\LICENSE"
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!define MUI_FINISHPAGE_RUN "$INSTDIR\libexec\tocin.exe"
!define MUI_FINISHPAGE_RUN_PARAMETERS "--version"
!define MUI_FINISHPAGE_RUN_TEXT "Show Tocin version"
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "English"

; --- Main component (required) ---------------------------------------------
Section "Tocin compiler + runtime (required)" SecMain
  SectionIn RO
  SetOutPath "$INSTDIR"

  ; Staged payload: libexec\ (tocin.exe + DLLs + link\ bundle) and stdlib\ are
  ; required. lib\ and share\ are optional: the Windows staging script
  ; (Build-TocinInstaller.ps1) puts every DLL next to tocin.exe in libexec\
  ; and creates no lib\ at all, so lib\ must not be a hard requirement.
  File /r "${STAGE}\libexec"
  File /nonfatal /r "${STAGE}\lib"
  File /r "${STAGE}\stdlib"
  File /nonfatal /r "${STAGE}\share"
  File /oname=LICENSE.txt "..\..\LICENSE"

  ; A tiny bin\tocin.cmd launcher that sets TOCIN_PATH then forwards args, so
  ; the stdlib is found no matter the working directory.
  CreateDirectory "$INSTDIR\bin"
  FileOpen $0 "$INSTDIR\bin\tocin.cmd" w
  FileWrite $0 "@echo off$\r$\n"
  FileWrite $0 "set $\"TOCIN_PATH=%~dp0..\stdlib$\"$\r$\n"
  FileWrite $0 "$\"%~dp0..\libexec\tocin.exe$\" %*$\r$\n"
  FileClose $0

  WriteUninstaller "$INSTDIR\uninstall.exe"

  ; Start-menu shortcuts.
  CreateDirectory "$SMPROGRAMS\Tocin"
  CreateShortcut "$SMPROGRAMS\Tocin\Tocin (command prompt).lnk" "$SYSDIR\cmd.exe" '/K "set PATH=$INSTDIR\bin;%PATH%"'
  CreateShortcut "$SMPROGRAMS\Tocin\Uninstall.lnk" "$INSTDIR\uninstall.exe"

  ; Registry: install dir + Add/Remove Programs entry.
  WriteRegStr HKLM "Software\Tocin" "Install_Dir" "$INSTDIR"
  WriteRegStr HKLM "${UNINST_KEY}" "DisplayName" "${PRODUCT_NAME}"
  WriteRegStr HKLM "${UNINST_KEY}" "UninstallString" '"$INSTDIR\uninstall.exe"'
  WriteRegStr HKLM "${UNINST_KEY}" "DisplayIcon" "$INSTDIR\libexec\tocin.exe"
  WriteRegStr HKLM "${UNINST_KEY}" "DisplayVersion" "${VERSION}"
  WriteRegStr HKLM "${UNINST_KEY}" "Publisher" "${PUBLISHER}"
  WriteRegStr HKLM "${UNINST_KEY}" "URLInfoAbout" "${WEBSITE}"
  WriteRegDWORD HKLM "${UNINST_KEY}" "NoModify" 1
  WriteRegDWORD HKLM "${UNINST_KEY}" "NoRepair" 1
SectionEnd

; --- Add to system PATH (optional) -----------------------------------------
Section "Add Tocin to PATH" SecPath
  ; Append $INSTDIR\bin to the machine PATH (idempotent) and broadcast the
  ; change so new shells pick it up without a reboot.
  ReadRegStr $1 HKLM "System\CurrentControlSet\Control\Session Manager\Environment" "Path"
  ${If} $1 == ""
    StrCpy $2 "$INSTDIR\bin"
  ${Else}
    StrCpy $2 "$1;$INSTDIR\bin"
  ${EndIf}
  WriteRegExpandStr HKLM "System\CurrentControlSet\Control\Session Manager\Environment" "Path" "$2"
  SendMessage ${HWND_BROADCAST} ${WM_WININICHANGE} 0 "STR:Environment" /TIMEOUT=5000
SectionEnd

; --- Associate .to files (optional) ----------------------------------------
Section "Associate .to files" SecAssoc
  WriteRegStr HKCR ".to" "" "Tocin.Source"
  WriteRegStr HKCR "Tocin.Source" "" "Tocin source file"
  WriteRegStr HKCR "Tocin.Source\DefaultIcon" "" "$INSTDIR\libexec\tocin.exe,0"
  WriteRegStr HKCR "Tocin.Source\shell\run\command" "" '"$INSTDIR\libexec\tocin.exe" "%1" --run'
SectionEnd

LangString DESC_Main  ${LANG_ENGLISH} "The Tocin compiler, runtime, standard library, and bundled linker."
LangString DESC_Path  ${LANG_ENGLISH} "Add Tocin to the system PATH so 'tocin' works in any terminal."
LangString DESC_Assoc ${LANG_ENGLISH} "Right-click a .to file to run it with Tocin."
!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
  !insertmacro MUI_DESCRIPTION_TEXT ${SecMain}  $(DESC_Main)
  !insertmacro MUI_DESCRIPTION_TEXT ${SecPath}  $(DESC_Path)
  !insertmacro MUI_DESCRIPTION_TEXT ${SecAssoc} $(DESC_Assoc)
!insertmacro MUI_FUNCTION_DESCRIPTION_END

; --- Uninstaller ------------------------------------------------------------
Section "Uninstall"
  RMDir /r "$INSTDIR\libexec"
  RMDir /r "$INSTDIR\lib"
  RMDir /r "$INSTDIR\stdlib"
  RMDir /r "$INSTDIR\share"
  RMDir /r "$INSTDIR\bin"
  Delete "$INSTDIR\LICENSE.txt"
  Delete "$INSTDIR\uninstall.exe"
  RMDir "$INSTDIR"

  Delete "$SMPROGRAMS\Tocin\*.*"
  RMDir "$SMPROGRAMS\Tocin"

  DeleteRegKey HKLM "${UNINST_KEY}"
  DeleteRegKey HKLM "Software\Tocin"
  DeleteRegKey HKCR ".to"
  DeleteRegKey HKCR "Tocin.Source"
  ; Note: PATH entry is left in place (safe to remove manually); rewriting the
  ; machine PATH from an uninstaller risks clobbering unrelated edits.
SectionEnd
