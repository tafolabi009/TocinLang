; CPack install script designed for a nmake build

;--------------------------------
; You must define these values

  !define VERSION "1.0.0"
  !define PATCH  "0"
  !define INST_DIR "D:/Downloads/tocin-compiler/_CPack_Packages/win64-Source/NSIS/Tocin-1.0.0-Source"

;--------------------------------
;Variables

  Var MUI_TEMP
  Var STARTMENU_FOLDER
  Var SV_ALLUSERS
  Var START_MENU
  Var DO_NOT_ADD_TO_PATH
  Var ADD_TO_PATH_ALL_USERS
  Var ADD_TO_PATH_CURRENT_USER
  Var INSTALL_DESKTOP
  Var IS_DEFAULT_INSTALLDIR
;--------------------------------
;Include Modern UI

  !include "MUI.nsh"

  ;Default installation folder
  InstallDir "$PROGRAMFILES64\Tocin Compiler"

;--------------------------------
;General

  ;Name and file
  Name "Tocin Compiler"
  OutFile "D:/Downloads/tocin-compiler/_CPack_Packages/win64-Source/NSIS/Tocin-1.0.0-Source.exe"

  ;Set compression
  SetCompressor lzma

  ;Require administrator access
  RequestExecutionLevel admin





  !include Sections.nsh

;--- Component support macros: ---
; The code for the add/remove functionality is from:
;   https://nsis.sourceforge.io/Add/Remove_Functionality
; It has been modified slightly and extended to provide
; inter-component dependencies.
Var AR_SecFlags
Var AR_RegFlags


; Loads the "selected" flag for the section named SecName into the
; variable VarName.
!macro LoadSectionSelectedIntoVar SecName VarName
 SectionGetFlags ${${SecName}} $${VarName}
 IntOp $${VarName} $${VarName} & ${SF_SELECTED}  ;Turn off all other bits
!macroend

; Loads the value of a variable... can we get around this?
!macro LoadVar VarName
  IntOp $R0 0 + $${VarName}
!macroend

; Sets the value of a variable
!macro StoreVar VarName IntValue
  IntOp $${VarName} 0 + ${IntValue}
!macroend

!macro InitSection SecName
  ;  This macro reads component installed flag from the registry and
  ;changes checked state of the section on the components page.
  ;Input: section index constant name specified in Section command.

  ClearErrors
  ;Reading component status from registry
  ReadRegDWORD $AR_RegFlags HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Tocin Compiler\Components\${SecName}" "Installed"
  IfErrors "default_${SecName}"
    ;Status will stay default if registry value not found
    ;(component was never installed)
  IntOp $AR_RegFlags $AR_RegFlags & ${SF_SELECTED} ;Turn off all other bits
  SectionGetFlags ${${SecName}} $AR_SecFlags  ;Reading default section flags
  IntOp $AR_SecFlags $AR_SecFlags & 0xFFFE  ;Turn lowest (enabled) bit off
  IntOp $AR_SecFlags $AR_RegFlags | $AR_SecFlags      ;Change lowest bit

  ; Note whether this component was installed before
  !insertmacro StoreVar ${SecName}_was_installed $AR_RegFlags
  IntOp $R0 $AR_RegFlags & $AR_RegFlags

  ;Writing modified flags
  SectionSetFlags ${${SecName}} $AR_SecFlags

 "default_${SecName}:"
 !insertmacro LoadSectionSelectedIntoVar ${SecName} ${SecName}_selected
!macroend

!macro FinishSection SecName
  ;  This macro reads section flag set by user and removes the section
  ;if it is not selected.
  ;Then it writes component installed flag to registry
  ;Input: section index constant name specified in Section command.

  SectionGetFlags ${${SecName}} $AR_SecFlags  ;Reading section flags
  ;Checking lowest bit:
  IntOp $AR_SecFlags $AR_SecFlags & ${SF_SELECTED}
  IntCmp $AR_SecFlags 1 "leave_${SecName}"
    ;Section is not selected:
    ;Calling Section uninstall macro and writing zero installed flag
    !insertmacro "Remove_${${SecName}}"
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Tocin Compiler\Components\${SecName}" \
  "Installed" 0
    Goto "exit_${SecName}"

 "leave_${SecName}:"
    ;Section is selected:
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Tocin Compiler\Components\${SecName}" \
  "Installed" 1

 "exit_${SecName}:"
!macroend

!macro RemoveSection_CPack SecName
  ;  This macro is used to call section's Remove_... macro
  ;from the uninstaller.
  ;Input: section index constant name specified in Section command.

  !insertmacro "Remove_${${SecName}}"
!macroend

; Determine whether the selection of SecName changed
!macro MaybeSelectionChanged SecName
  !insertmacro LoadVar ${SecName}_selected
  SectionGetFlags ${${SecName}} $R1
  IntOp $R1 $R1 & ${SF_SELECTED} ;Turn off all other bits

  ; See if the status has changed:
  IntCmp $R0 $R1 "${SecName}_unchanged"
  !insertmacro LoadSectionSelectedIntoVar ${SecName} ${SecName}_selected

  IntCmp $R1 ${SF_SELECTED} "${SecName}_was_selected"
  !insertmacro "Deselect_required_by_${SecName}"
  goto "${SecName}_unchanged"

  "${SecName}_was_selected:"
  !insertmacro "Select_${SecName}_depends"

  "${SecName}_unchanged:"
!macroend
;--- End of Add/Remove macros ---

;--------------------------------
;Interface Settings

  !define MUI_HEADERIMAGE
  !define MUI_ABORTWARNING

;----------------------------------------
; based upon a script of "Written by KiCHiK 2003-01-18 05:57:02"
;----------------------------------------
!verbose 3
!include "WinMessages.NSH"
!verbose 4
;====================================================
; get_NT_environment
;     Returns: the selected environment
;     Output : head of the stack
;====================================================
!macro select_NT_profile UN
Function ${UN}select_NT_profile
   StrCmp $ADD_TO_PATH_ALL_USERS "1" 0 environment_single
      DetailPrint "Selected environment for all users"
      Push "all"
      Return
   environment_single:
      DetailPrint "Selected environment for current user only."
      Push "current"
      Return
FunctionEnd
!macroend
!insertmacro select_NT_profile ""
!insertmacro select_NT_profile "un."
;----------------------------------------------------
!define NT_current_env 'HKCU "Environment"'
!define NT_all_env     'HKLM "SYSTEM\CurrentControlSet\Control\Session Manager\Environment"'

!ifndef WriteEnvStr_RegKey
  !ifdef ALL_USERS
    !define WriteEnvStr_RegKey \
       'HKLM "SYSTEM\CurrentControlSet\Control\Session Manager\Environment"'
  !else
    !define WriteEnvStr_RegKey 'HKCU "Environment"'
  !endif
!endif

; AddToPath - Adds the given dir to the search path.
;        Input - head of the stack
;        Note - Win9x systems requires reboot

Function AddToPath
  Exch $0
  Push $1
  Push $2
  Push $3

  # don't add if the path doesn't exist
  IfFileExists "$0\*.*" "" AddToPath_done

  ReadEnvStr $1 PATH
  ; if the path is too long for a NSIS variable NSIS will return a 0
  ; length string.  If we find that, then warn and skip any path
  ; modification as it will trash the existing path.
  StrLen $2 $1
  IntCmp $2 0 CheckPathLength_ShowPathWarning CheckPathLength_Done CheckPathLength_Done
    CheckPathLength_ShowPathWarning:
    Messagebox MB_OK|MB_ICONEXCLAMATION "Warning! PATH too long installer unable to modify PATH!"
    Goto AddToPath_done
  CheckPathLength_Done:
  Push "$1;"
  Push "$0;"
  Call StrStr
  Pop $2
  StrCmp $2 "" "" AddToPath_done
  Push "$1;"
  Push "$0\;"
  Call StrStr
  Pop $2
  StrCmp $2 "" "" AddToPath_done
  GetFullPathName /SHORT $3 $0
  Push "$1;"
  Push "$3;"
  Call StrStr
  Pop $2
  StrCmp $2 "" "" AddToPath_done
  Push "$1;"
  Push "$3\;"
  Call StrStr
  Pop $2
  StrCmp $2 "" "" AddToPath_done

  Call IsNT
  Pop $1
  StrCmp $1 1 AddToPath_NT
    ; Not on NT
    StrCpy $1 $WINDIR 2
    FileOpen $1 "$1\autoexec.bat" a
    FileSeek $1 -1 END
    FileReadByte $1 $2
    IntCmp $2 26 0 +2 +2 # DOS EOF
      FileSeek $1 -1 END # write over EOF
    FileWrite $1 "$\r$\nSET PATH=%PATH%;$3$\r$\n"
    FileClose $1
    SetRebootFlag true
    Goto AddToPath_done

  AddToPath_NT:
    StrCmp $ADD_TO_PATH_ALL_USERS "1" ReadAllKey
      ReadRegStr $1 ${NT_current_env} "PATH"
      Goto DoTrim
    ReadAllKey:
      ReadRegStr $1 ${NT_all_env} "PATH"
    DoTrim:
    StrCmp $1 "" AddToPath_NTdoIt
      Push $1
      Call Trim
      Pop $1
      StrCpy $0 "$1;$0"
    AddToPath_NTdoIt:
      StrCmp $ADD_TO_PATH_ALL_USERS "1" WriteAllKey
        WriteRegExpandStr ${NT_current_env} "PATH" $0
        Goto DoSend
      WriteAllKey:
        WriteRegExpandStr ${NT_all_env} "PATH" $0
      DoSend:
      SendMessage ${HWND_BROADCAST} ${WM_WININICHANGE} 0 "STR:Environment" /TIMEOUT=5000

  AddToPath_done:
    Pop $3
    Pop $2
    Pop $1
    Pop $0
FunctionEnd


; RemoveFromPath - Remove a given dir from the path
;     Input: head of the stack

Function un.RemoveFromPath
  Exch $0
  Push $1
  Push $2
  Push $3
  Push $4
  Push $5
  Push $6

  IntFmt $6 "%c" 26 # DOS EOF

  Call un.IsNT
  Pop $1
  StrCmp $1 1 unRemoveFromPath_NT
    ; Not on NT
    StrCpy $1 $WINDIR 2
    FileOpen $1 "$1\autoexec.bat" r
    GetTempFileName $4
    FileOpen $2 $4 w
    GetFullPathName /SHORT $0 $0
    StrCpy $0 "SET PATH=%PATH%;$0"
    Goto unRemoveFromPath_dosLoop

    unRemoveFromPath_dosLoop:
      FileRead $1 $3
      StrCpy $5 $3 1 -1 # read last char
      StrCmp $5 $6 0 +2 # if DOS EOF
        StrCpy $3 $3 -1 # remove DOS EOF so we can compare
      StrCmp $3 "$0$\r$\n" unRemoveFromPath_dosLoopRemoveLine
      StrCmp $3 "$0$\n" unRemoveFromPath_dosLoopRemoveLine
      StrCmp $3 "$0" unRemoveFromPath_dosLoopRemoveLine
      StrCmp $3 "" unRemoveFromPath_dosLoopEnd
      FileWrite $2 $3
      Goto unRemoveFromPath_dosLoop
      unRemoveFromPath_dosLoopRemoveLine:
        SetRebootFlag true
        Goto unRemoveFromPath_dosLoop

    unRemoveFromPath_dosLoopEnd:
      FileClose $2
      FileClose $1
      StrCpy $1 $WINDIR 2
      Delete "$1\autoexec.bat"
      CopyFiles /SILENT $4 "$1\autoexec.bat"
      Delete $4
      Goto unRemoveFromPath_done

  unRemoveFromPath_NT:
    StrCmp $ADD_TO_PATH_ALL_USERS "1" unReadAllKey
      ReadRegStr $1 ${NT_current_env} "PATH"
      Goto unDoTrim
    unReadAllKey:
      ReadRegStr $1 ${NT_all_env} "PATH"
    unDoTrim:
    StrCpy $5 $1 1 -1 # copy last char
    StrCmp $5 ";" +2 # if last char != ;
      StrCpy $1 "$1;" # append ;
    Push $1
    Push "$0;"
    Call un.StrStr ; Find `$0;` in $1
    Pop $2 ; pos of our dir
    StrCmp $2 "" unRemoveFromPath_done
      ; else, it is in path
      # $0 - path to add
      # $1 - path var
      StrLen $3 "$0;"
      StrLen $4 $2
      StrCpy $5 $1 -$4 # $5 is now the part before the path to remove
      StrCpy $6 $2 "" $3 # $6 is now the part after the path to remove
      StrCpy $3 $5$6

      StrCpy $5 $3 1 -1 # copy last char
      StrCmp $5 ";" 0 +2 # if last char == ;
        StrCpy $3 $3 -1 # remove last char

      StrCmp $ADD_TO_PATH_ALL_USERS "1" unWriteAllKey
        WriteRegExpandStr ${NT_current_env} "PATH" $3
        Goto unDoSend
      unWriteAllKey:
        WriteRegExpandStr ${NT_all_env} "PATH" $3
      unDoSend:
      SendMessage ${HWND_BROADCAST} ${WM_WININICHANGE} 0 "STR:Environment" /TIMEOUT=5000

  unRemoveFromPath_done:
    Pop $6
    Pop $5
    Pop $4
    Pop $3
    Pop $2
    Pop $1
    Pop $0
FunctionEnd

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; Uninstall stuff
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

###########################################
#            Utility Functions            #
###########################################

;====================================================
; IsNT - Returns 1 if the current system is NT, 0
;        otherwise.
;     Output: head of the stack
;====================================================
; IsNT
; no input
; output, top of the stack = 1 if NT or 0 if not
;
; Usage:
;   Call IsNT
;   Pop $R0
;  ($R0 at this point is 1 or 0)

!macro IsNT un
Function ${un}IsNT
  Push $0
  ReadRegStr $0 HKLM "SOFTWARE\Microsoft\Windows NT\CurrentVersion" CurrentVersion
  StrCmp $0 "" 0 IsNT_yes
  ; we are not NT.
  Pop $0
  Push 0
  Return

  IsNT_yes:
    ; NT!!!
    Pop $0
    Push 1
FunctionEnd
!macroend
!insertmacro IsNT ""
!insertmacro IsNT "un."

; StrStr
; input, top of stack = string to search for
;        top of stack-1 = string to search in
; output, top of stack (replaces with the portion of the string remaining)
; modifies no other variables.
;
; Usage:
;   Push "this is a long ass string"
;   Push "ass"
;   Call StrStr
;   Pop $R0
;  ($R0 at this point is "ass string")

!macro StrStr un
Function ${un}StrStr
Exch $R1 ; st=haystack,old$R1, $R1=needle
  Exch    ; st=old$R1,haystack
  Exch $R2 ; st=old$R1,old$R2, $R2=haystack
  Push $R3
  Push $R4
  Push $R5
  StrLen $R3 $R1
  StrCpy $R4 0
  ; $R1=needle
  ; $R2=haystack
  ; $R3=len(needle)
  ; $R4=cnt
  ; $R5=tmp
  loop:
    StrCpy $R5 $R2 $R3 $R4
    StrCmp $R5 $R1 done
    StrCmp $R5 "" done
    IntOp $R4 $R4 + 1
    Goto loop
done:
  StrCpy $R1 $R2 "" $R4
  Pop $R5
  Pop $R4
  Pop $R3
  Pop $R2
  Exch $R1
FunctionEnd
!macroend
!insertmacro StrStr ""
!insertmacro StrStr "un."

Function Trim ; Added by Pelaca
	Exch $R1
	Push $R2
Loop:
	StrCpy $R2 "$R1" 1 -1
	StrCmp "$R2" " " RTrim
	StrCmp "$R2" "$\n" RTrim
	StrCmp "$R2" "$\r" RTrim
	StrCmp "$R2" ";" RTrim
	GoTo Done
RTrim:
	StrCpy $R1 "$R1" -1
	Goto Loop
Done:
	Pop $R2
	Exch $R1
FunctionEnd

Function ConditionalAddToRegistry
  Pop $0
  Pop $1
  StrCmp "$0" "" ConditionalAddToRegistry_EmptyString
    WriteRegStr SHCTX "Software\Microsoft\Windows\CurrentVersion\Uninstall\Tocin Compiler" \
    "$1" "$0"
    ;MessageBox MB_OK "Set Registry: '$1' to '$0'"
    DetailPrint "Set install registry entry: '$1' to '$0'"
  ConditionalAddToRegistry_EmptyString:
FunctionEnd

;--------------------------------

!ifdef CPACK_USES_DOWNLOAD
Function DownloadFile
    IfFileExists $INSTDIR\* +2
    CreateDirectory $INSTDIR
    Pop $0

    ; Skip if already downloaded
    IfFileExists $INSTDIR\$0 0 +2
    Return

    StrCpy $1 ""

  try_again:
    NSISdl::download "$1/$0" "$INSTDIR\$0"

    Pop $1
    StrCmp $1 "success" success
    StrCmp $1 "Cancelled" cancel
    MessageBox MB_OK "Download failed: $1"
  cancel:
    Return
  success:
FunctionEnd
!endif

;--------------------------------
; Define some macro setting for the gui
!define MUI_ICON "D:/Downloads/tocin-compiler/Tocin_Logo.ico"
!define MUI_UNICON "D:/Downloads/tocin-compiler/Tocin_Logo.ico"






;--------------------------------
;Pages
  
  
  !insertmacro MUI_PAGE_WELCOME

  !insertmacro MUI_PAGE_LICENSE "D:/Downloads/msys64/mingw64/share/cmake/Templates/CPack.GenericLicense.txt"

  Page custom InstallOptionsPage
  !insertmacro MUI_PAGE_DIRECTORY

  ;Start Menu Folder Page Configuration
  !define MUI_STARTMENUPAGE_REGISTRY_ROOT "SHCTX"
  !define MUI_STARTMENUPAGE_REGISTRY_KEY "Software\Tocin Team\Tocin Compiler"
  !define MUI_STARTMENUPAGE_REGISTRY_VALUENAME "Start Menu Folder"
  !insertmacro MUI_PAGE_STARTMENU Application $STARTMENU_FOLDER

  

  !insertmacro MUI_PAGE_INSTFILES
  
  
  !insertmacro MUI_PAGE_FINISH

  !insertmacro MUI_UNPAGE_CONFIRM
  !insertmacro MUI_UNPAGE_INSTFILES

;--------------------------------
;Languages

  !insertmacro MUI_LANGUAGE "English" ;first language is the default language
  !insertmacro MUI_LANGUAGE "Afrikaans"
  !insertmacro MUI_LANGUAGE "Albanian"
  !insertmacro MUI_LANGUAGE "Arabic"
  !insertmacro MUI_LANGUAGE "Asturian"
  !insertmacro MUI_LANGUAGE "Basque"
  !insertmacro MUI_LANGUAGE "Belarusian"
  !insertmacro MUI_LANGUAGE "Bosnian"
  !insertmacro MUI_LANGUAGE "Breton"
  !insertmacro MUI_LANGUAGE "Bulgarian"
  !insertmacro MUI_LANGUAGE "Catalan"
  !insertmacro MUI_LANGUAGE "Corsican"
  !insertmacro MUI_LANGUAGE "Croatian"
  !insertmacro MUI_LANGUAGE "Czech"
  !insertmacro MUI_LANGUAGE "Danish"
  !insertmacro MUI_LANGUAGE "Dutch"
  !insertmacro MUI_LANGUAGE "Esperanto"
  !insertmacro MUI_LANGUAGE "Estonian"
  !insertmacro MUI_LANGUAGE "Farsi"
  !insertmacro MUI_LANGUAGE "Finnish"
  !insertmacro MUI_LANGUAGE "French"
  !insertmacro MUI_LANGUAGE "Galician"
  !insertmacro MUI_LANGUAGE "German"
  !insertmacro MUI_LANGUAGE "Greek"
  !insertmacro MUI_LANGUAGE "Hebrew"
  !insertmacro MUI_LANGUAGE "Hungarian"
  !insertmacro MUI_LANGUAGE "Icelandic"
  !insertmacro MUI_LANGUAGE "Indonesian"
  !insertmacro MUI_LANGUAGE "Irish"
  !insertmacro MUI_LANGUAGE "Italian"
  !insertmacro MUI_LANGUAGE "Japanese"
  !insertmacro MUI_LANGUAGE "Korean"
  !insertmacro MUI_LANGUAGE "Kurdish"
  !insertmacro MUI_LANGUAGE "Latvian"
  !insertmacro MUI_LANGUAGE "Lithuanian"
  !insertmacro MUI_LANGUAGE "Luxembourgish"
  !insertmacro MUI_LANGUAGE "Macedonian"
  !insertmacro MUI_LANGUAGE "Malay"
  !insertmacro MUI_LANGUAGE "Mongolian"
  !insertmacro MUI_LANGUAGE "Norwegian"
  !insertmacro MUI_LANGUAGE "NorwegianNynorsk"
  !insertmacro MUI_LANGUAGE "Pashto"
  !insertmacro MUI_LANGUAGE "Polish"
  !insertmacro MUI_LANGUAGE "Portuguese"
  !insertmacro MUI_LANGUAGE "PortugueseBR"
  !insertmacro MUI_LANGUAGE "Romanian"
  !insertmacro MUI_LANGUAGE "Russian"
  !insertmacro MUI_LANGUAGE "ScotsGaelic"
  !insertmacro MUI_LANGUAGE "Serbian"
  !insertmacro MUI_LANGUAGE "SerbianLatin"
  !insertmacro MUI_LANGUAGE "SimpChinese"
  !insertmacro MUI_LANGUAGE "Slovak"
  !insertmacro MUI_LANGUAGE "Slovenian"
  !insertmacro MUI_LANGUAGE "Spanish"
  !insertmacro MUI_LANGUAGE "SpanishInternational"
  !insertmacro MUI_LANGUAGE "Swedish"
  !insertmacro MUI_LANGUAGE "Tatar"
  !insertmacro MUI_LANGUAGE "Thai"
  !insertmacro MUI_LANGUAGE "TradChinese"
  !insertmacro MUI_LANGUAGE "Turkish"
  !insertmacro MUI_LANGUAGE "Ukrainian"
  !insertmacro MUI_LANGUAGE "Uzbek"
  !insertmacro MUI_LANGUAGE "Vietnamese"
  !insertmacro MUI_LANGUAGE "Welsh"

;--------------------------------
;Reserve Files

  ;These files should be inserted before other files in the data block
  ;Keep these lines before any File command
  ;Only for solid compression (by default, solid compression is enabled for BZIP2 and LZMA)

  ReserveFile "NSIS.InstallOptions.ini"
  !insertmacro MUI_RESERVEFILE_INSTALLOPTIONS

  ; for UserInfo::GetName and UserInfo::GetAccountType
  ReserveFile /plugin 'UserInfo.dll'

;--------------------------------
; Installation types


;--------------------------------
; Component sections


;--------------------------------
;Installer Sections

Section "-Core installation"
  ;Use the entire tree produced by the INSTALL target.  Keep the
  ;list of directories here in sync with the RMDir commands below.
  SetOutPath "$INSTDIR"
  
  File /r "${INST_DIR}\*.*"

  ;Store installation folder
  WriteRegStr SHCTX "Software\Tocin Team\Tocin Compiler" "" $INSTDIR

  ;Create uninstaller
  WriteUninstaller "$INSTDIR\Uninstall.exe"
  Push "DisplayName"
  Push "Tocin Compiler"
  Call ConditionalAddToRegistry
  Push "DisplayVersion"
  Push "1.0.0"
  Call ConditionalAddToRegistry
  Push "Publisher"
  Push "Tocin Team"
  Call ConditionalAddToRegistry
  Push "UninstallString"
  Push "$\"$INSTDIR\Uninstall.exe$\""
  Call ConditionalAddToRegistry
  Push "NoRepair"
  Push "1"
  Call ConditionalAddToRegistry

  !ifdef CPACK_NSIS_ADD_REMOVE
  ;Create add/remove functionality
  Push "ModifyPath"
  Push "$INSTDIR\AddRemove.exe"
  Call ConditionalAddToRegistry
  !else
  Push "NoModify"
  Push "1"
  Call ConditionalAddToRegistry
  !endif

  ; Optional registration
  Push "DisplayIcon"
  Push "$INSTDIR\"
  Call ConditionalAddToRegistry
  Push "HelpLink"
  Push ""
  Call ConditionalAddToRegistry
  Push "URLInfoAbout"
  Push ""
  Call ConditionalAddToRegistry
  Push "Contact"
  Push ""
  Call ConditionalAddToRegistry
  !insertmacro MUI_INSTALLOPTIONS_READ $INSTALL_DESKTOP "NSIS.InstallOptions.ini" "Field 5" "State"
  !insertmacro MUI_STARTMENU_WRITE_BEGIN Application

  ;Create shortcuts
  CreateDirectory "$SMPROGRAMS\$STARTMENU_FOLDER"
  WriteINIStr "$SMPROGRAMS\$STARTMENU_FOLDER\Website.url" "InternetShortcut" "URL" "https://tocin.vercel.app"
  WriteINIStr "$SMPROGRAMS\$STARTMENU_FOLDER\GitHub.url" "InternetShortcut" "URL" "https://github.com/tafolabi009/tocin_compiler"


  CreateShortCut "$SMPROGRAMS\$STARTMENU_FOLDER\Uninstall.lnk" "$INSTDIR\Uninstall.exe"

  ;Read a value from an InstallOptions INI file
  !insertmacro MUI_INSTALLOPTIONS_READ $DO_NOT_ADD_TO_PATH "NSIS.InstallOptions.ini" "Field 2" "State"
  !insertmacro MUI_INSTALLOPTIONS_READ $ADD_TO_PATH_ALL_USERS "NSIS.InstallOptions.ini" "Field 3" "State"
  !insertmacro MUI_INSTALLOPTIONS_READ $ADD_TO_PATH_CURRENT_USER "NSIS.InstallOptions.ini" "Field 4" "State"

  ; Write special uninstall registry entries
  Push "StartMenu"
  Push "$STARTMENU_FOLDER"
  Call ConditionalAddToRegistry
  Push "DoNotAddToPath"
  Push "$DO_NOT_ADD_TO_PATH"
  Call ConditionalAddToRegistry
  Push "AddToPathAllUsers"
  Push "$ADD_TO_PATH_ALL_USERS"
  Call ConditionalAddToRegistry
  Push "AddToPathCurrentUser"
  Push "$ADD_TO_PATH_CURRENT_USER"
  Call ConditionalAddToRegistry
  Push "InstallToDesktop"
  Push "$INSTALL_DESKTOP"
  Call ConditionalAddToRegistry

  !insertmacro MUI_STARTMENU_WRITE_END


        CreateShortCut "$DESKTOP\Tocin Compiler.lnk" "$INSTDIR\tocin.exe"
    

SectionEnd

Section "-Add to path"
  Push $INSTDIR\bin
  StrCmp "ON" "ON" 0 doNotAddToPath
  StrCmp $DO_NOT_ADD_TO_PATH "1" doNotAddToPath 0
    Call AddToPath
  doNotAddToPath:
SectionEnd

;--------------------------------
; Create custom pages
Function InstallOptionsPage
  !insertmacro MUI_HEADER_TEXT "Install Options" "Choose options for installing Tocin Compiler"
  !insertmacro MUI_INSTALLOPTIONS_DISPLAY "NSIS.InstallOptions.ini"

FunctionEnd

;--------------------------------
; determine admin versus local install
Function un.onInit

  ClearErrors
  UserInfo::GetName
  IfErrors noLM
  Pop $0
  UserInfo::GetAccountType
  Pop $1
  StrCmp $1 "Admin" 0 +3
    SetShellVarContext all
    ;MessageBox MB_OK 'User "$0" is in the Admin group'
    Goto done
  StrCmp $1 "Power" 0 +3
    SetShellVarContext all
    ;MessageBox MB_OK 'User "$0" is in the Power Users group'
    Goto done

  noLM:
    ;Get installation folder from registry if available

  done:

FunctionEnd

;--- Add/Remove callback functions: ---
!macro SectionList MacroName
  ;This macro used to perform operation on multiple sections.
  ;List all of your components in following manner here.

!macroend

Section -FinishComponents
  ;Removes unselected components and writes component status to registry
  !insertmacro SectionList "FinishSection"

!ifdef CPACK_NSIS_ADD_REMOVE
  ; Get the name of the installer executable
  System::Call 'kernel32::GetModuleFileNameA(i 0, t .R0, i 1024) i r1'
  StrCpy $R3 $R0

  ; Strip off the last 13 characters, to see if we have AddRemove.exe
  StrLen $R1 $R0
  IntOp $R1 $R0 - 13
  StrCpy $R2 $R0 13 $R1
  StrCmp $R2 "AddRemove.exe" addremove_installed

  ; We're not running AddRemove.exe, so install it
  CopyFiles $R3 $INSTDIR\AddRemove.exe

  addremove_installed:
!endif
SectionEnd
;--- End of Add/Remove callback functions ---

;--------------------------------
; Component dependencies
Function .onSelChange
  !insertmacro SectionList MaybeSelectionChanged
FunctionEnd

;--------------------------------
;Uninstaller Section

Section "Uninstall"
  ReadRegStr $START_MENU SHCTX \
   "Software\Microsoft\Windows\CurrentVersion\Uninstall\Tocin Compiler" "StartMenu"
  ;MessageBox MB_OK "Start menu is in: $START_MENU"
  ReadRegStr $DO_NOT_ADD_TO_PATH SHCTX \
    "Software\Microsoft\Windows\CurrentVersion\Uninstall\Tocin Compiler" "DoNotAddToPath"
  ReadRegStr $ADD_TO_PATH_ALL_USERS SHCTX \
    "Software\Microsoft\Windows\CurrentVersion\Uninstall\Tocin Compiler" "AddToPathAllUsers"
  ReadRegStr $ADD_TO_PATH_CURRENT_USER SHCTX \
    "Software\Microsoft\Windows\CurrentVersion\Uninstall\Tocin Compiler" "AddToPathCurrentUser"
  ;MessageBox MB_OK "Add to path: $DO_NOT_ADD_TO_PATH all users: $ADD_TO_PATH_ALL_USERS"
  ReadRegStr $INSTALL_DESKTOP SHCTX \
    "Software\Microsoft\Windows\CurrentVersion\Uninstall\Tocin Compiler" "InstallToDesktop"
  ;MessageBox MB_OK "Install to desktop: $INSTALL_DESKTOP "


        Delete "$DESKTOP\Tocin Compiler.lnk"
    

  ;Remove files we installed.
  ;Keep the list of directories here in sync with the File commands above.
  Delete "$INSTDIR\.cursor"
  Delete "$INSTDIR\.cursor\commands"
  Delete "$INSTDIR\.cursor\commands\create-contex.md"
  Delete "$INSTDIR\.gitattributes"
  Delete "$INSTDIR\.github"
  Delete "$INSTDIR\.github\workflows"
  Delete "$INSTDIR\.github\workflows\ci.yml"
  Delete "$INSTDIR\.gitignore"
  Delete "$INSTDIR\.history"
  Delete "$INSTDIR\.history\build_instructions_20250514100419.txt"
  Delete "$INSTDIR\.history\build_instructions_20250514100449.txt"
  Delete "$INSTDIR\.history\build_instructions_20250514100450.txt"
  Delete "$INSTDIR\.history\build_tocin_20250514100349.bat"
  Delete "$INSTDIR\.history\build_tocin_20250514100354.bat"
  Delete "$INSTDIR\.history\compiler_fixes_20250514102047.md"
  Delete "$INSTDIR\.history\compiler_fixes_20250514102101.md"
  Delete "$INSTDIR\.history\compiler_fixes_20250514102103.md"
  Delete "$INSTDIR\.history\README_20250307183653.md"
  Delete "$INSTDIR\.history\README_20250514100231.md"
  Delete "$INSTDIR\.history\README_20250514100233.md"
  Delete "$INSTDIR\.history\src"
  Delete "$INSTDIR\.history\src\ast"
  Delete "$INSTDIR\.history\src\ast\ast_20250512174210.h"
  Delete "$INSTDIR\.history\src\ast\ast_20250512174223.h"
  Delete "$INSTDIR\.history\src\ast\ast_20250512174224.h"
  Delete "$INSTDIR\.history\src\ast\ast_20250512174225.h"
  Delete "$INSTDIR\.history\src\codegen"
  Delete "$INSTDIR\.history\src\codegen\ir_generator_20250511142531.h"
  Delete "$INSTDIR\.history\src\codegen\ir_generator_20250511142534.h"
  Delete "$INSTDIR\.history\src\codegen\ir_generator_20250511142535.h"
  Delete "$INSTDIR\.history\src\codegen\ir_generator_20250511142732.cpp"
  Delete "$INSTDIR\.history\src\codegen\ir_generator_20250511142736.cpp"
  Delete "$INSTDIR\.history\src\codegen\ir_generator_20250511142751.cpp"
  Delete "$INSTDIR\.history\src\codegen\ir_generator_20250511142752.cpp"
  Delete "$INSTDIR\.history\src\codegen\ir_generator_20250511142753.cpp"
  Delete "$INSTDIR\.history\src\codegen\ir_generator_20250511144202.h"
  Delete "$INSTDIR\.history\src\codegen\ir_generator_20250511144204.h"
  Delete "$INSTDIR\.history\src\codegen\ir_generator_20250511144330.cpp"
  Delete "$INSTDIR\.history\src\codegen\ir_generator_20250511144331.cpp"
  Delete "$INSTDIR\.history\src\codegen\ir_generator_20250511144334.cpp"
  Delete "$INSTDIR\.history\src\codegen\ir_generator_20250511144340.cpp"
  Delete "$INSTDIR\.history\src\codegen\ir_generator_20250511144505.cpp"
  Delete "$INSTDIR\.history\src\codegen\ir_generator_20250511144508.cpp"
  Delete "$INSTDIR\.history\src\codegen\ir_generator_20250512113203.cpp"
  Delete "$INSTDIR\.history\src\codegen\ir_generator_20250512113204.cpp"
  Delete "$INSTDIR\.history\src\codegen\ir_generator_20250512113205.cpp"
  Delete "$INSTDIR\.history\src\codegen\ir_generator_20250512171511.cpp"
  Delete "$INSTDIR\.history\src\codegen\ir_generator_20250512171512.cpp"
  Delete "$INSTDIR\.history\src\codegen\ir_generator_20250512171514.cpp"
  Delete "$INSTDIR\.history\src\codegen\ir_generator_20250512171516.cpp"
  Delete "$INSTDIR\.history\src\codegen\ir_generator_20250512171540.cpp"
  Delete "$INSTDIR\.history\src\codegen\ir_generator_20250512171541.cpp"
  Delete "$INSTDIR\.history\src\codegen\ir_generator_20250512171542.cpp"
  Delete "$INSTDIR\.history\src\codegen\ir_generator_20250512171547.cpp"
  Delete "$INSTDIR\.history\src\codegen\ir_generator_20250512171614.cpp"
  Delete "$INSTDIR\.history\src\codegen\ir_generator_20250512171626.cpp"
  Delete "$INSTDIR\.history\src\codegen\ir_generator_20250512171627.cpp"
  Delete "$INSTDIR\.history\src\codegen\ir_generator_20250512171628.cpp"
  Delete "$INSTDIR\.history\src\codegen\ir_generator_20250512171629.cpp"
  Delete "$INSTDIR\.history\src\codegen\ir_generator_20250512171630.cpp"
  Delete "$INSTDIR\.history\src\codegen\ir_generator_20250512171631.cpp"
  Delete "$INSTDIR\.history\src\codegen\ir_generator_20250512171632.cpp"
  Delete "$INSTDIR\.history\src\codegen\ir_generator_20250512171640.cpp"
  Delete "$INSTDIR\.history\src\codegen\ir_generator_20250512171709.cpp"
  Delete "$INSTDIR\.history\src\codegen\ir_generator_20250512171710.cpp"
  Delete "$INSTDIR\.history\src\codegen\ir_generator_20250512171711.cpp"
  Delete "$INSTDIR\.history\src\codegen\ir_generator_20250512171712.cpp"
  Delete "$INSTDIR\.history\src\codegen\ir_generator_20250512171713.cpp"
  Delete "$INSTDIR\.history\src\codegen\ir_generator_20250512171714.cpp"
  Delete "$INSTDIR\.history\src\codegen\ir_generator_20250512171715.cpp"
  Delete "$INSTDIR\.history\src\codegen\ir_generator_20250512171734.cpp"
  Delete "$INSTDIR\.history\src\codegen\ir_generator_20250512171746.cpp"
  Delete "$INSTDIR\.history\src\codegen\ir_generator_20250512171747.cpp"
  Delete "$INSTDIR\.history\src\codegen\ir_generator_20250512171748.cpp"
  Delete "$INSTDIR\.history\src\codegen\ir_generator_20250512171749.cpp"
  Delete "$INSTDIR\.history\src\codegen\ir_generator_20250512171750.cpp"
  Delete "$INSTDIR\.history\src\codegen\ir_generator_20250512171751.cpp"
  Delete "$INSTDIR\.history\src\codegen\ir_generator_20250512171756.cpp"
  Delete "$INSTDIR\.history\src\codegen\ir_generator_20250512171838.cpp"
  Delete "$INSTDIR\.history\src\codegen\ir_generator_20250512171839.cpp"
  Delete "$INSTDIR\.history\src\codegen\ir_generator_20250512171840.cpp"
  Delete "$INSTDIR\.history\src\codegen\ir_generator_20250512171841.cpp"
  Delete "$INSTDIR\.history\src\codegen\ir_generator_20250512171842.cpp"
  Delete "$INSTDIR\.history\src\codegen\ir_generator_20250512171847.cpp"
  Delete "$INSTDIR\.history\src\codegen\ir_generator_20250512171903.h"
  Delete "$INSTDIR\.history\src\codegen\ir_generator_20250512171905.h"
  Delete "$INSTDIR\.history\src\codegen\ir_generator_20250512172604.cpp"
  Delete "$INSTDIR\.history\src\codegen\ir_generator_20250512172644.cpp"
  Delete "$INSTDIR\.history\src\codegen\ir_generator_20250512172648.cpp"
  Delete "$INSTDIR\.history\src\codegen\ir_generator_20250512172713.cpp"
  Delete "$INSTDIR\.history\src\codegen\ir_generator_20250512172805.h"
  Delete "$INSTDIR\.history\src\codegen\ir_generator_20250512174105.h"
  Delete "$INSTDIR\.history\src\codegen\ir_generator_20250512174106.h"
  Delete "$INSTDIR\.history\src\codegen\ir_generator_20250512174110.h"
  Delete "$INSTDIR\.history\src\codegen\ir_generator_20250513102342.cpp"
  Delete "$INSTDIR\.history\src\codegen\ir_generator_20250513102344.cpp"
  Delete "$INSTDIR\.history\src\codegen\ir_generator_20250513102508.cpp"
  Delete "$INSTDIR\.history\src\codegen\ir_generator_20250513102511.cpp"
  Delete "$INSTDIR\.history\src\codegen\ir_generator_20250513102512.cpp"
  Delete "$INSTDIR\.history\src\codegen\ir_generator_20250513102513.cpp"
  Delete "$INSTDIR\.history\src\codegen\ir_generator_20250513102525.cpp"
  Delete "$INSTDIR\.history\src\codegen\ir_generator_20250513102527.cpp"
  Delete "$INSTDIR\.history\src\codegen\ir_generator_20250513102528.cpp"
  Delete "$INSTDIR\.history\src\codegen\ir_generator_20250513105836.cpp"
  Delete "$INSTDIR\.history\src\codegen\ir_generator_20250513130752.cpp"
  Delete "$INSTDIR\.history\src\codegen\ir_generator_20250513130758.cpp"
  Delete "$INSTDIR\.history\src\codegen\ir_generator_20250513130759.cpp"
  Delete "$INSTDIR\.history\src\codegen\ir_generator_20250513132124.cpp"
  Delete "$INSTDIR\.history\src\codegen\ir_generator_20250513132229.h"
  Delete "$INSTDIR\.history\src\codegen\ir_generator_20250513132307.cpp"
  Delete "$INSTDIR\.history\src\codegen\ir_generator_20250513132310.cpp"
  Delete "$INSTDIR\.history\src\codegen\ir_generator_20250513132337.cpp"
  Delete "$INSTDIR\.history\src\codegen\ir_generator_20250513132339.cpp"
  Delete "$INSTDIR\.history\src\codegen\ir_generator_20250513132408.h"
  Delete "$INSTDIR\.history\src\codegen\ir_generator_20250513132413.h"
  Delete "$INSTDIR\.history\src\error"
  Delete "$INSTDIR\.history\src\error\error_handler_20250514095418.h"
  Delete "$INSTDIR\.history\src\error\error_handler_20250514095423.h"
  Delete "$INSTDIR\.history\src\ffi"
  Delete "$INSTDIR\.history\src\ffi\ffi_enhanced_20250512183637.h"
  Delete "$INSTDIR\.history\src\ffi\ffi_enhanced_20250512183642.h"
  Delete "$INSTDIR\.history\src\ffi\ffi_enhanced_20250512183644.h"
  Delete "$INSTDIR\.history\src\main_20250512171128.cpp"
  Delete "$INSTDIR\.history\src\main_20250512171132.cpp"
  Delete "$INSTDIR\.history\src\main_20250512171134.cpp"
  Delete "$INSTDIR\.history\src\runtime"
  Delete "$INSTDIR\.history\src\runtime\concurrency_20250513102159.h"
  Delete "$INSTDIR\.history\src\runtime\concurrency_20250513102204.h"
  Delete "$INSTDIR\.history\src\runtime\concurrency_20250513102208.h"
  Delete "$INSTDIR\.history\src\runtime\concurrency_20250513102224.h"
  Delete "$INSTDIR\.history\src\runtime\concurrency_20250513102228.h"
  Delete "$INSTDIR\.history\src\runtime\concurrency_20250513102229.h"
  Delete "$INSTDIR\.history\src\runtime\concurrency_20250513102247.h"
  Delete "$INSTDIR\.history\src\runtime\concurrency_20250513102256.h"
  Delete "$INSTDIR\.history\src\type"
  Delete "$INSTDIR\.history\src\type\type_checker_20250511140017.cpp"
  Delete "$INSTDIR\.history\src\type\type_checker_20250511140020.cpp"
  Delete "$INSTDIR\.history\src\type\type_checker_20250511140022.cpp"
  Delete "$INSTDIR\.history\src\type\type_checker_20250511140427.cpp"
  Delete "$INSTDIR\.history\src\type\type_checker_20250511140441.cpp"
  Delete "$INSTDIR\.history\src\type\type_checker_20250511140442.cpp"
  Delete "$INSTDIR\.history\src\type\type_checker_20250511140444.cpp"
  Delete "$INSTDIR\.history\src\type\type_checker_20250511140447.cpp"
  Delete "$INSTDIR\.history\src\type\type_checker_20250511140451.cpp"
  Delete "$INSTDIR\.history\src\type\type_checker_20250511140519.cpp"
  Delete "$INSTDIR\.history\src\type\type_checker_20250511140537.cpp"
  Delete "$INSTDIR\.history\src\type\type_checker_20250511140556.cpp"
  Delete "$INSTDIR\.history\src\type\type_checker_20250511140602.cpp"
  Delete "$INSTDIR\.history\src\type\type_checker_20250511140613.cpp"
  Delete "$INSTDIR\.history\src\type\type_checker_20250511140702.cpp"
  Delete "$INSTDIR\.history\src\type\type_checker_20250511140712.cpp"
  Delete "$INSTDIR\.history\src\type\type_checker_20250511140715.cpp"
  Delete "$INSTDIR\.history\src\type\type_checker_20250511140732.cpp"
  Delete "$INSTDIR\.history\src\type\type_checker_20250511140735.cpp"
  Delete "$INSTDIR\.history\src\type\type_checker_20250511140737.cpp"
  Delete "$INSTDIR\.history\src\type\type_checker_20250511140739.cpp"
  Delete "$INSTDIR\.history\src\type\type_checker_20250511140749.cpp"
  Delete "$INSTDIR\.history\src\type\type_checker_20250511140914.cpp"
  Delete "$INSTDIR\.history\src\type\type_checker_20250511141016.cpp"
  Delete "$INSTDIR\.history\src\type\type_checker_20250511141020.cpp"
  Delete "$INSTDIR\.history\src\type\type_checker_20250511141320.cpp"
  Delete "$INSTDIR\.history\src\type\type_checker_20250511141925.cpp"
  Delete "$INSTDIR\.history\src\type\type_checker_20250514230229.cpp"
  Delete "$INSTDIR\.history\src\type\type_checker_20250514230230.cpp"
  Delete "$INSTDIR\.history\src\type\type_checker_20250515000046.cpp"
  Delete "$INSTDIR\.history\src\type\type_checker_20250515000052.cpp"
  Delete "$INSTDIR\.history\src\type\type_checker_20250515000241.cpp"
  Delete "$INSTDIR\.history\src\type\type_checker_20250515000244.cpp"
  Delete "$INSTDIR\.history\src\type\type_checker_20250515001332.cpp"
  Delete "$INSTDIR\.history\src\type\type_checker_20250515001333.cpp"
  Delete "$INSTDIR\.history\src\type\type_checker_20250515001623.cpp"
  Delete "$INSTDIR\.history\src\type\type_checker_20250515001624.cpp"
  Delete "$INSTDIR\.history\src\type\type_checker_20250515002248.cpp"
  Delete "$INSTDIR\.history\src\type\type_checker_20250515002250.cpp"
  Delete "$INSTDIR\.history\src\type\type_checker_20250515002314.cpp"
  Delete "$INSTDIR\.history\src\type\type_checker_20250515002346.cpp"
  Delete "$INSTDIR\.history\src\type\type_checker_20250515002406.cpp"
  Delete "$INSTDIR\.history\src\type\type_checker_20250515002407.cpp"
  Delete "$INSTDIR\.history\src\type\type_checker_20250515002436.cpp"
  Delete "$INSTDIR\.history\stdlib"
  Delete "$INSTDIR\.history\stdlib\fs"
  Delete "$INSTDIR\.history\stdlib\fs\fs_20250512182953.to"
  Delete "$INSTDIR\.history\stdlib\fs\fs_20250512182958.to"
  Delete "$INSTDIR\.history\stdlib\fs\fs_20250512182959.to"
  Delete "$INSTDIR\.history\stdlib\fs\fs_20250512183001.to"
  Delete "$INSTDIR\.history\stdlib\math"
  Delete "$INSTDIR\.history\stdlib\math\math_20250512182852.to"
  Delete "$INSTDIR\.history\stdlib\math\math_20250512182856.to"
  Delete "$INSTDIR\.history\stdlib\math\math_20250512182858.to"
  Delete "$INSTDIR\.history\stdlib\math\math_20250512182859.to"
  Delete "$INSTDIR\.history\stdlib\ml"
  Delete "$INSTDIR\.history\stdlib\ml\basic_20250512183345.to"
  Delete "$INSTDIR\.history\stdlib\ml\basic_20250512183354.to"
  Delete "$INSTDIR\.history\stdlib\ml\basic_20250512183355.to"
  Delete "$INSTDIR\.history\stdlib\ml\basic_20250512183407.to"
  Delete "$INSTDIR\.history\stdlib\ml\basic_20250512183411.to"
  Delete "$INSTDIR\.history\stdlib\ml\basic_20250512183412.to"
  Delete "$INSTDIR\.history\stdlib\ml\basic_20250512183430.to"
  Delete "$INSTDIR\.history\stdlib\ml\basic_20250512183432.to"
  Delete "$INSTDIR\.history\stdlib\ml\basic_20250512183433.to"
  Delete "$INSTDIR\.history\stdlib\ml\basic_20250512183434.to"
  Delete "$INSTDIR\.history\stdlib\net"
  Delete "$INSTDIR\.history\stdlib\net\socket_20250512183508.to"
  Delete "$INSTDIR\.history\stdlib\net\socket_20250512183511.to"
  Delete "$INSTDIR\.history\stdlib\net\socket_20250512183513.to"
  Delete "$INSTDIR\.history\stdlib\pkg"
  Delete "$INSTDIR\.history\stdlib\pkg\manager_20250512183614.to"
  Delete "$INSTDIR\.history\stdlib\pkg\manager_20250512183622.to"
  Delete "$INSTDIR\.history\stdlib\pkg\manager_20250512183624.to"
  Delete "$INSTDIR\.history\stdlib\web"
  Delete "$INSTDIR\.history\stdlib\web\http_20250512183045.to"
  Delete "$INSTDIR\.history\stdlib\web\http_20250512183058.to"
  Delete "$INSTDIR\.history\stdlib\web\http_20250512183100.to"
  Delete "$INSTDIR\.history\tocin-compiler"
  Delete "$INSTDIR\.history\tocin-compiler\build_20250511133727.sh"
  Delete "$INSTDIR\.history\tocin-compiler\build_20250511133730.sh"
  Delete "$INSTDIR\.history\tocin-compiler\build_20250511133731.sh"
  Delete "$INSTDIR\.history\tocin-compiler\build_20250512230043.bat"
  Delete "$INSTDIR\.history\tocin-compiler\build_20250512230046.bat"
  Delete "$INSTDIR\.history\tocin-compiler\build_20250512230047.bat"
  Delete "$INSTDIR\.history\tocin-compiler\build_20250512230106.ps1"
  Delete "$INSTDIR\.history\tocin-compiler\build_20250512230109.ps1"
  Delete "$INSTDIR\.history\tocin-compiler\build_20250512230110.ps1"
  Delete "$INSTDIR\.history\tocin-compiler\build_20250512230111.ps1"
  Delete "$INSTDIR\.history\tocin-compiler\build_20250512230202.ps1"
  Delete "$INSTDIR\.history\tocin-compiler\build_20250512230222.bat"
  Delete "$INSTDIR\.history\tocin-compiler\build_20250512230309.ps1"
  Delete "$INSTDIR\.history\tocin-compiler\build_20250512230316.ps1"
  Delete "$INSTDIR\.history\tocin-compiler\build_20250512230330.bat"
  Delete "$INSTDIR\.history\tocin-compiler\build_20250512230610.bat"
  Delete "$INSTDIR\.history\tocin-compiler\BUILD_INSTRUCTIONS_20250512230134.md"
  Delete "$INSTDIR\.history\tocin-compiler\BUILD_INSTRUCTIONS_20250512230146.md"
  Delete "$INSTDIR\.history\tocin-compiler\BUILD_INSTRUCTIONS_20250512230225.md"
  Delete "$INSTDIR\.history\tocin-compiler\BUILD_INSTRUCTIONS_20250513055314.md"
  Delete "$INSTDIR\.history\tocin-compiler\BUILD_INSTRUCTIONS_20250513055315.md"
  Delete "$INSTDIR\.history\tocin-compiler\BUILD_INSTRUCTIONS_20250513055537.md"
  Delete "$INSTDIR\.history\tocin-compiler\BUILD_INSTRUCTIONS_20250513061037.md"
  Delete "$INSTDIR\.history\tocin-compiler\BUILD_INSTRUCTIONS_20250513061116.md"
  Delete "$INSTDIR\.history\tocin-compiler\BUILD_INSTRUCTIONS_20250513065744.md"
  Delete "$INSTDIR\.history\tocin-compiler\BUILD_INSTRUCTIONS_20250513065745.md"
  Delete "$INSTDIR\.history\tocin-compiler\BUILD_INSTRUCTIONS_20250513085652.md"
  Delete "$INSTDIR\.history\tocin-compiler\build_project_20250512170228.sh"
  Delete "$INSTDIR\.history\tocin-compiler\build_project_20250512170232.sh"
  Delete "$INSTDIR\.history\tocin-compiler\build_project_20250512170233.sh"
  Delete "$INSTDIR\.history\tocin-compiler\build_with_llvm_20250513053032.ps1"
  Delete "$INSTDIR\.history\tocin-compiler\build_with_llvm_20250513053043.ps1"
  Delete "$INSTDIR\.history\tocin-compiler\build_with_llvm_20250513053045.ps1"
  Delete "$INSTDIR\.history\tocin-compiler\build_with_llvm_20250513060748.ps1"
  Delete "$INSTDIR\.history\tocin-compiler\build_with_llvm_20250513060749.ps1"
  Delete "$INSTDIR\.history\tocin-compiler\build_with_llvm_20250513060751.ps1"
  Delete "$INSTDIR\.history\tocin-compiler\CMakeLists_20250421120202.txt"
  Delete "$INSTDIR\.history\tocin-compiler\CMakeLists_20250511014356.txt"
  Delete "$INSTDIR\.history\tocin-compiler\CMakeLists_20250511014358.txt"
  Delete "$INSTDIR\.history\tocin-compiler\CMakeLists_20250511014517.txt"
  Delete "$INSTDIR\.history\tocin-compiler\CMakeLists_20250511014518.txt"
  Delete "$INSTDIR\.history\tocin-compiler\CMakeLists_20250511015152.txt"
  Delete "$INSTDIR\.history\tocin-compiler\CMakeLists_20250511015153.txt"
  Delete "$INSTDIR\.history\tocin-compiler\CMakeLists_20250511093350.txt"
  Delete "$INSTDIR\.history\tocin-compiler\CMakeLists_20250511093352.txt"
  Delete "$INSTDIR\.history\tocin-compiler\CMakeLists_20250511093400.txt"
  Delete "$INSTDIR\.history\tocin-compiler\CMakeLists_20250511093402.txt"
  Delete "$INSTDIR\.history\tocin-compiler\CMakeLists_20250511093403.txt"
  Delete "$INSTDIR\.history\tocin-compiler\CMakeLists_20250512122546.txt"
  Delete "$INSTDIR\.history\tocin-compiler\CMakeLists_20250512122547.txt"
  Delete "$INSTDIR\.history\tocin-compiler\CMakeLists_20250512122605.txt"
  Delete "$INSTDIR\.history\tocin-compiler\CMakeLists_20250512122606.txt"
  Delete "$INSTDIR\.history\tocin-compiler\CMakeLists_20250512122900.txt"
  Delete "$INSTDIR\.history\tocin-compiler\CMakeLists_20250512155117.txt"
  Delete "$INSTDIR\.history\tocin-compiler\CMakeLists_20250512160705.txt"
  Delete "$INSTDIR\.history\tocin-compiler\CMakeLists_20250512160923.txt"
  Delete "$INSTDIR\.history\tocin-compiler\CMakeLists_20250512160925.txt"
  Delete "$INSTDIR\.history\tocin-compiler\CMakeLists_20250512160931.txt"
  Delete "$INSTDIR\.history\tocin-compiler\CMakeLists_20250512160934.txt"
  Delete "$INSTDIR\.history\tocin-compiler\CMakeLists_20250512160937.txt"
  Delete "$INSTDIR\.history\tocin-compiler\CMakeLists_20250512161432.txt"
  Delete "$INSTDIR\.history\tocin-compiler\CMakeLists_20250512161433.txt"
  Delete "$INSTDIR\.history\tocin-compiler\CMakeLists_20250512161451.txt"
  Delete "$INSTDIR\.history\tocin-compiler\CMakeLists_20250512162939.txt"
  Delete "$INSTDIR\.history\tocin-compiler\CMakeLists_20250512162950.txt"
  Delete "$INSTDIR\.history\tocin-compiler\CMakeLists_20250512163433.txt"
  Delete "$INSTDIR\.history\tocin-compiler\CMakeLists_20250512163434.txt"
  Delete "$INSTDIR\.history\tocin-compiler\CMakeLists_20250512163435.txt"
  Delete "$INSTDIR\.history\tocin-compiler\CMakeLists_20250512163436.txt"
  Delete "$INSTDIR\.history\tocin-compiler\CMakeLists_20250512163437.txt"
  Delete "$INSTDIR\.history\tocin-compiler\CMakeLists_20250512163438.txt"
  Delete "$INSTDIR\.history\tocin-compiler\CMakeLists_20250512163439.txt"
  Delete "$INSTDIR\.history\tocin-compiler\CMakeLists_20250512164055.txt"
  Delete "$INSTDIR\.history\tocin-compiler\CMakeLists_20250512164232.txt"
  Delete "$INSTDIR\.history\tocin-compiler\CMakeLists_20250512164538.txt"
  Delete "$INSTDIR\.history\tocin-compiler\CMakeLists_20250512164539.txt"
  Delete "$INSTDIR\.history\tocin-compiler\CMakeLists_20250512164732.txt"
  Delete "$INSTDIR\.history\tocin-compiler\CMakeLists_20250512164901.txt"
  Delete "$INSTDIR\.history\tocin-compiler\CMakeLists_20250512165118.txt"
  Delete "$INSTDIR\.history\tocin-compiler\CMakeLists_20250512165119.txt"
  Delete "$INSTDIR\.history\tocin-compiler\CMakeLists_20250512165440.txt"
  Delete "$INSTDIR\.history\tocin-compiler\CMakeLists_20250512165442.txt"
  Delete "$INSTDIR\.history\tocin-compiler\CMakeLists_20250512165443.txt"
  Delete "$INSTDIR\.history\tocin-compiler\CMakeLists_20250512165449.txt"
  Delete "$INSTDIR\.history\tocin-compiler\CMakeLists_20250512165743.txt"
  Delete "$INSTDIR\.history\tocin-compiler\CMakeLists_20250512165807.txt"
  Delete "$INSTDIR\.history\tocin-compiler\CMakeLists_20250512170129.txt"
  Delete "$INSTDIR\.history\tocin-compiler\CMakeLists_20250512170157.txt"
  Delete "$INSTDIR\.history\tocin-compiler\CMakeLists_20250512170901.txt"
  Delete "$INSTDIR\.history\tocin-compiler\CMakeLists_20250512170902.txt"
  Delete "$INSTDIR\.history\tocin-compiler\CMakeLists_20250512170917.txt"
  Delete "$INSTDIR\.history\tocin-compiler\CMakeLists_20250512224915.txt"
  Delete "$INSTDIR\.history\tocin-compiler\CMakeLists_20250512225140.txt"
  Delete "$INSTDIR\.history\tocin-compiler\CMakeLists_20250512225143.txt"
  Delete "$INSTDIR\.history\tocin-compiler\CMakeLists_20250512225221.txt"
  Delete "$INSTDIR\.history\tocin-compiler\CMakeLists_20250512225225.txt"
  Delete "$INSTDIR\.history\tocin-compiler\CMakeLists_20250513044258.txt"
  Delete "$INSTDIR\.history\tocin-compiler\CMakeLists_20250513044310.txt"
  Delete "$INSTDIR\.history\tocin-compiler\CMakeLists_20250513045449.txt"
  Delete "$INSTDIR\.history\tocin-compiler\CMakeLists_20250513045451.txt"
  Delete "$INSTDIR\.history\tocin-compiler\CMakeLists_20250513052602.txt"
  Delete "$INSTDIR\.history\tocin-compiler\CMakeLists_20250513052603.txt"
  Delete "$INSTDIR\.history\tocin-compiler\CMakeLists_20250513052612.txt"
  Delete "$INSTDIR\.history\tocin-compiler\CMakeLists_20250513052619.txt"
  Delete "$INSTDIR\.history\tocin-compiler\CMakeLists_20250513052630.txt"
  Delete "$INSTDIR\.history\tocin-compiler\CMakeLists_20250513052639.txt"
  Delete "$INSTDIR\.history\tocin-compiler\CMakeLists_20250513052643.txt"
  Delete "$INSTDIR\.history\tocin-compiler\CMakeLists_20250513052650.txt"
  Delete "$INSTDIR\.history\tocin-compiler\CMakeLists_20250513052654.txt"
  Delete "$INSTDIR\.history\tocin-compiler\CMakeLists_20250513052659.txt"
  Delete "$INSTDIR\.history\tocin-compiler\CMakeLists_20250513053000.txt"
  Delete "$INSTDIR\.history\tocin-compiler\CMakeLists_20250513053003.txt"
  Delete "$INSTDIR\.history\tocin-compiler\CMakeLists_20250513053010.txt"
  Delete "$INSTDIR\.history\tocin-compiler\CMakeLists_20250513053014.txt"
  Delete "$INSTDIR\.history\tocin-compiler\CMakeLists_20250513053018.txt"
  Delete "$INSTDIR\.history\tocin-compiler\CMakeLists_20250513101416.txt"
  Delete "$INSTDIR\.history\tocin-compiler\CMakeLists_20250513101417.txt"
  Delete "$INSTDIR\.history\tocin-compiler\CMakeLists_20250513101419.txt"
  Delete "$INSTDIR\.history\tocin-compiler\CMakePresets_20250228045412.json"
  Delete "$INSTDIR\.history\tocin-compiler\CMakePresets_20250511093631.json"
  Delete "$INSTDIR\.history\tocin-compiler\CMakePresets_20250511093632.json"
  Delete "$INSTDIR\.history\tocin-compiler\compile_verify_llvm_20250513091306.bat"
  Delete "$INSTDIR\.history\tocin-compiler\compile_verify_llvm_20250513091330.bat"
  Delete "$INSTDIR\.history\tocin-compiler\compile_verify_llvm_20250513091331.bat"
  Delete "$INSTDIR\.history\tocin-compiler\compile_verify_llvm_20250513091433.bat"
  Delete "$INSTDIR\.history\tocin-compiler\CONCLUSION_20250513070045.md"
  Delete "$INSTDIR\.history\tocin-compiler\CONCLUSION_20250513070051.md"
  Delete "$INSTDIR\.history\tocin-compiler\CONCLUSION_20250513070052.md"
  Delete "$INSTDIR\.history\tocin-compiler\direct_build_20250513052713.bat"
  Delete "$INSTDIR\.history\tocin-compiler\direct_build_20250513052720.bat"
  Delete "$INSTDIR\.history\tocin-compiler\direct_build_20250513052736.ps1"
  Delete "$INSTDIR\.history\tocin-compiler\direct_build_20250513052745.ps1"
  Delete "$INSTDIR\.history\tocin-compiler\direct_build_20250513052748.ps1"
  Delete "$INSTDIR\.history\tocin-compiler\direct_build_20250513052827.ps1"
  Delete "$INSTDIR\.history\tocin-compiler\direct_build_20250513052828.ps1"
  Delete "$INSTDIR\.history\tocin-compiler\direct_build_20250513052842.ps1"
  Delete "$INSTDIR\.history\tocin-compiler\direct_build_20250513052847.ps1"
  Delete "$INSTDIR\.history\tocin-compiler\direct_build_20250513060849.ps1"
  Delete "$INSTDIR\.history\tocin-compiler\direct_build_20250513060850.ps1"
  Delete "$INSTDIR\.history\tocin-compiler\direct_build_20250513060852.ps1"
  Delete "$INSTDIR\.history\tocin-compiler\docs"
  Delete "$INSTDIR\.history\tocin-compiler\docs\01_Introduction_20250512204226.md"
  Delete "$INSTDIR\.history\tocin-compiler\docs\01_Introduction_20250512204230.md"
  Delete "$INSTDIR\.history\tocin-compiler\docs\01_Introduction_20250512204232.md"
  Delete "$INSTDIR\.history\tocin-compiler\docs\01_Introduction_20250512204234.md"
  Delete "$INSTDIR\.history\tocin-compiler\docs\01_Introduction_20250512204236.md"
  Delete "$INSTDIR\.history\tocin-compiler\docs\01_Introduction_20250512204746.md"
  Delete "$INSTDIR\.history\tocin-compiler\docs\02_Getting_Started_20250512204258.md"
  Delete "$INSTDIR\.history\tocin-compiler\docs\02_Getting_Started_20250512204302.md"
  Delete "$INSTDIR\.history\tocin-compiler\docs\02_Getting_Started_20250512204304.md"
  Delete "$INSTDIR\.history\tocin-compiler\docs\02_Getting_Started_20250512204307.md"
  Delete "$INSTDIR\.history\tocin-compiler\docs\02_Getting_Started_20250512204309.md"
  Delete "$INSTDIR\.history\tocin-compiler\docs\02_Getting_Started_20250512204310.md"
  Delete "$INSTDIR\.history\tocin-compiler\docs\02_Getting_Started_20250512204751.md"
  Delete "$INSTDIR\.history\tocin-compiler\docs\03_Language_Basics_20250512204417.md"
  Delete "$INSTDIR\.history\tocin-compiler\docs\03_Language_Basics_20250512204426.md"
  Delete "$INSTDIR\.history\tocin-compiler\docs\03_Language_Basics_20250512204427.md"
  Delete "$INSTDIR\.history\tocin-compiler\docs\03_Language_Basics_20250512204430.md"
  Delete "$INSTDIR\.history\tocin-compiler\docs\03_Language_Basics_20250512204432.md"
  Delete "$INSTDIR\.history\tocin-compiler\docs\03_Language_Basics_20250512204434.md"
  Delete "$INSTDIR\.history\tocin-compiler\docs\03_Language_Basics_20250512204754.md"
  Delete "$INSTDIR\.history\tocin-compiler\docs\04_Standard_Library_20250512204722.md"
  Delete "$INSTDIR\.history\tocin-compiler\docs\04_Standard_Library_20250512204731.md"
  Delete "$INSTDIR\.history\tocin-compiler\docs\04_Standard_Library_20250512204734.md"
  Delete "$INSTDIR\.history\tocin-compiler\docs\04_Standard_Library_20250512222732.md"
  Delete "$INSTDIR\.history\tocin-compiler\docs\04_Standard_Library_20250512222734.md"
  Delete "$INSTDIR\.history\tocin-compiler\docs\04_Standard_Library_20250512222807.md"
  Delete "$INSTDIR\.history\tocin-compiler\docs\04_Standard_Library_20250512222843.md"
  Delete "$INSTDIR\.history\tocin-compiler\docs\04_Standard_Library_20250512222851.md"
  Delete "$INSTDIR\.history\tocin-compiler\docs\05_Advanced_Topics_20250512223231.md"
  Delete "$INSTDIR\.history\tocin-compiler\docs\05_Advanced_Topics_20250512223237.md"
  Delete "$INSTDIR\.history\tocin-compiler\docs\05_Advanced_Topics_20250512223246.md"
  Delete "$INSTDIR\.history\tocin-compiler\docs\CONCURRENCY_20250511020045.md"
  Delete "$INSTDIR\.history\tocin-compiler\docs\CONCURRENCY_20250511020048.md"
  Delete "$INSTDIR\.history\tocin-compiler\docs\CONCURRENCY_20250511020050.md"
  Delete "$INSTDIR\.history\tocin-compiler\docs\LANGUAGE_FEATURES_20250511020115.md"
  Delete "$INSTDIR\.history\tocin-compiler\docs\LANGUAGE_FEATURES_20250511020119.md"
  Delete "$INSTDIR\.history\tocin-compiler\docs\LANGUAGE_FEATURES_20250511020121.md"
  Delete "$INSTDIR\.history\tocin-compiler\docs\OPTION_RESULT_TYPES_20250511014954.md"
  Delete "$INSTDIR\.history\tocin-compiler\docs\OPTION_RESULT_TYPES_20250511014959.md"
  Delete "$INSTDIR\.history\tocin-compiler\docs\OPTION_RESULT_TYPES_20250511015000.md"
  Delete "$INSTDIR\.history\tocin-compiler\examples"
  Delete "$INSTDIR\.history\tocin-compiler\examples\advanced_20250511012129.tocin"
  Delete "$INSTDIR\.history\tocin-compiler\examples\advanced_20250511012133.tocin"
  Delete "$INSTDIR\.history\tocin-compiler\examples\advanced_20250511012134.tocin"
  Delete "$INSTDIR\.history\tocin-compiler\examples\advanced_20250511012139.tocin"
  Delete "$INSTDIR\.history\tocin-compiler\examples\advanced_20250511012140.tocin"
  Delete "$INSTDIR\.history\tocin-compiler\examples\advanced_20250511012142.tocin"
  Delete "$INSTDIR\.history\tocin-compiler\examples\concurrency_demo_20250511020020.tc"
  Delete "$INSTDIR\.history\tocin-compiler\examples\concurrency_demo_20250511020025.tc"
  Delete "$INSTDIR\.history\tocin-compiler\examples\concurrency_demo_20250511020026.tc"
  Delete "$INSTDIR\.history\tocin-compiler\examples\concurrency_demo_20250512202555.tc"
  Delete "$INSTDIR\.history\tocin-compiler\examples\concurrency_demo_20250512202616.to"
  Delete "$INSTDIR\.history\tocin-compiler\examples\concurrency_demo_20250512202620.to"
  Delete "$INSTDIR\.history\tocin-compiler\examples\concurrency_demo_20250512202622.to"
  Delete "$INSTDIR\.history\tocin-compiler\examples\fibonacci_20250511012118.tocin"
  Delete "$INSTDIR\.history\tocin-compiler\examples\fibonacci_20250511012123.tocin"
  Delete "$INSTDIR\.history\tocin-compiler\examples\fibonacci_20250511012124.tocin"
  Delete "$INSTDIR\.history\tocin-compiler\examples\game_demo_20250512203137.to"
  Delete "$INSTDIR\.history\tocin-compiler\examples\game_demo_20250512203153.to"
  Delete "$INSTDIR\.history\tocin-compiler\examples\game_demo_20250512203156.to"
  Delete "$INSTDIR\.history\tocin-compiler\examples\gui_demo_20250512202846.to"
  Delete "$INSTDIR\.history\tocin-compiler\examples\gui_demo_20250512202848.to"
  Delete "$INSTDIR\.history\tocin-compiler\examples\gui_demo_20250512202850.to"
  Delete "$INSTDIR\.history\tocin-compiler\examples\hello_world_20250511011751.tocin"
  Delete "$INSTDIR\.history\tocin-compiler\examples\hello_world_20250512202459.tocin"
  Delete "$INSTDIR\.history\tocin-compiler\examples\hello_world_20250512202500.tocin"
  Delete "$INSTDIR\.history\tocin-compiler\examples\ml_demo_20250512202921.to"
  Delete "$INSTDIR\.history\tocin-compiler\examples\ml_demo_20250512202928.to"
  Delete "$INSTDIR\.history\tocin-compiler\examples\ml_demo_20250512202929.to"
  Delete "$INSTDIR\.history\tocin-compiler\examples\ml_demo_20250512202930.to"
  Delete "$INSTDIR\.history\tocin-compiler\examples\option_result_demo_20250511014928.tc"
  Delete "$INSTDIR\.history\tocin-compiler\examples\option_result_demo_20250511014934.tc"
  Delete "$INSTDIR\.history\tocin-compiler\examples\option_result_demo_20250511014935.tc"
  Delete "$INSTDIR\.history\tocin-compiler\examples\option_result_demo_20250511014936.tc"
  Delete "$INSTDIR\.history\tocin-compiler\examples\traits_demo_20250511015854.tc"
  Delete "$INSTDIR\.history\tocin-compiler\examples\traits_demo_20250511015858.tc"
  Delete "$INSTDIR\.history\tocin-compiler\examples\traits_demo_20250511015859.tc"
  Delete "$INSTDIR\.history\tocin-compiler\examples\traits_demo_20250512202645.to"
  Delete "$INSTDIR\.history\tocin-compiler\examples\traits_demo_20250512202823.to"
  Delete "$INSTDIR\.history\tocin-compiler\examples\traits_demo_20250512202824.to"
  Delete "$INSTDIR\.history\tocin-compiler\examples\web_demo_20250512203031.to"
  Delete "$INSTDIR\.history\tocin-compiler\examples\web_demo_20250512203035.to"
  Delete "$INSTDIR\.history\tocin-compiler\examples\web_demo_20250512203037.to"
  Delete "$INSTDIR\.history\tocin-compiler\FEATURE_IMPLEMENTATION_SUMMARY_20250513101709.md"
  Delete "$INSTDIR\.history\tocin-compiler\FEATURE_IMPLEMENTATION_SUMMARY_20250513101714.md"
  Delete "$INSTDIR\.history\tocin-compiler\FEATURE_IMPLEMENTATION_SUMMARY_20250513101715.md"
  Delete "$INSTDIR\.history\tocin-compiler\fixed_code"
  Delete "$INSTDIR\.history\tocin-compiler\fixed_code\channel_methods_20250514164255.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\fixed_code\channel_methods_20250514164329.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\fixed_code\channel_methods_20250514164333.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\IMPLEMENTED_FEATURES_20250513101342.md"
  Delete "$INSTDIR\.history\tocin-compiler\IMPLEMENTED_FEATURES_20250513101350.md"
  Delete "$INSTDIR\.history\tocin-compiler\IMPLEMENTED_FEATURES_20250513101354.md"
  Delete "$INSTDIR\.history\tocin-compiler\IMPLEMENTED_FEATURES_20250513101356.md"
  Delete "$INSTDIR\.history\tocin-compiler\IMPLEMENTED_FEATURES_20250513101409.md"
  Delete "$INSTDIR\.history\tocin-compiler\IR_GENERATOR_STATUS_20250513061012.md"
  Delete "$INSTDIR\.history\tocin-compiler\IR_GENERATOR_STATUS_20250513061017.md"
  Delete "$INSTDIR\.history\tocin-compiler\IR_GENERATOR_STATUS_20250513061018.md"
  Delete "$INSTDIR\.history\tocin-compiler\IR_GENERATOR_STATUS_20250513065714.md"
  Delete "$INSTDIR\.history\tocin-compiler\IR_GENERATOR_STATUS_20250513065715.md"
  Delete "$INSTDIR\.history\tocin-compiler\IR_GENERATOR_STATUS_20250513085652.md"
  Delete "$INSTDIR\.history\tocin-compiler\IR_GENERATOR_STATUS_20250513090916.md"
  Delete "$INSTDIR\.history\tocin-compiler\IR_GENERATOR_STATUS_20250513091021.md"
  Delete "$INSTDIR\.history\tocin-compiler\LANGUAGE_ROADMAP_20250511014658.md"
  Delete "$INSTDIR\.history\tocin-compiler\LANGUAGE_ROADMAP_20250511014707.md"
  Delete "$INSTDIR\.history\tocin-compiler\LANGUAGE_ROADMAP_20250511014708.md"
  Delete "$INSTDIR\.history\tocin-compiler\LICENSE_20250512223350"
  Delete "$INSTDIR\.history\tocin-compiler\LICENSE_20250512223355"
  Delete "$INSTDIR\.history\tocin-compiler\LICENSE_20250512223357"
  Delete "$INSTDIR\.history\tocin-compiler\PROGRESS_REPORT_20250512230802.md"
  Delete "$INSTDIR\.history\tocin-compiler\PROGRESS_REPORT_20250512230825.md"
  Delete "$INSTDIR\.history\tocin-compiler\PROGRESS_REPORT_20250512230826.md"
  Delete "$INSTDIR\.history\tocin-compiler\PROGRESS_REPORT_20250512231547.md"
  Delete "$INSTDIR\.history\tocin-compiler\PROGRESS_REPORT_20250512231548.md"
  Delete "$INSTDIR\.history\tocin-compiler\PROGRESS_REPORT_20250513044235.md"
  Delete "$INSTDIR\.history\tocin-compiler\PROGRESS_REPORT_20250513044236.md"
  Delete "$INSTDIR\.history\tocin-compiler\PROGRESS_REPORT_20250513061144.md"
  Delete "$INSTDIR\.history\tocin-compiler\PROGRESS_REPORT_20250513061147.md"
  Delete "$INSTDIR\.history\tocin-compiler\PROGRESS_REPORT_20250513065840.md"
  Delete "$INSTDIR\.history\tocin-compiler\PROGRESS_REPORT_20250513065841.md"
  Delete "$INSTDIR\.history\tocin-compiler\README_20250511011734.md"
  Delete "$INSTDIR\.history\tocin-compiler\README_20250511011751.md"
  Delete "$INSTDIR\.history\tocin-compiler\README_20250511011752.md"
  Delete "$INSTDIR\.history\tocin-compiler\README_20250512184043.md"
  Delete "$INSTDIR\.history\tocin-compiler\README_20250512184044.md"
  Delete "$INSTDIR\.history\tocin-compiler\scripts"
  Delete "$INSTDIR\.history\tocin-compiler\scripts\build_20250511015730.bat"
  Delete "$INSTDIR\.history\tocin-compiler\scripts\build_20250511015743.bat"
  Delete "$INSTDIR\.history\tocin-compiler\scripts\install_dependencies_20250511014334.bat"
  Delete "$INSTDIR\.history\tocin-compiler\scripts\install_dependencies_20250511014342.bat"
  Delete "$INSTDIR\.history\tocin-compiler\scripts\install_dependencies_20250511014517.bat"
  Delete "$INSTDIR\.history\tocin-compiler\scripts\setup_dev_environment_20250511014608.bat"
  Delete "$INSTDIR\.history\tocin-compiler\scripts\setup_dev_environment_20250511014619.bat"
  Delete "$INSTDIR\.history\tocin-compiler\scripts\setup_dev_environment_20250511014629.bat"
  Delete "$INSTDIR\.history\tocin-compiler\simple_build_20250513052907.ps1"
  Delete "$INSTDIR\.history\tocin-compiler\simple_build_20250513052929.ps1"
  Delete "$INSTDIR\.history\tocin-compiler\simple_build_20250513052932.ps1"
  Delete "$INSTDIR\.history\tocin-compiler\simple_llvm_test_20250513070001.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\simple_llvm_test_20250513070006.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\simple_llvm_test_20250513070007.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\ast_20250421121415.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\ast_20250512123208.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\ast_20250512123209.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\ast_20250512123210.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\ast_20250512123213.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\ast_20250512123214.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\ast_20250512130347.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\ast_20250512130351.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\ast_20250512130433.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\ast_20250512132113.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\ast_20250512132120.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\ast_20250512132122.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\ast_20250512135550.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\ast_20250512135605.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\ast_20250512135607.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\ast_20250512143949.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\ast_20250512144035.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\ast_20250512144847.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\ast_20250512144849.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\ast_20250512150523.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\ast_20250512150525.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\ast_20250512150527.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\ast_20250512180248.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\ast_20250512180249.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\ast_20250512180251.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\ast_20250512180657.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\ast_20250512180658.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\ast_20250512180700.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\ast_20250512181922.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\ast_20250512181924.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\ast_20250512181931.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\ast_20250512231202.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\ast_20250512231203.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\ast_20250512231354.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\ast_20250512231355.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\ast_20250512231357.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\ast_20250512231502.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\ast_20250512231944.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\ast_20250512232043.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\ast_20250512232053.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\ast_20250512232206.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\ast_20250512232210.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\ast_20250512232220.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\ast_20250512232230.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\ast_20250513042145.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\ast_20250514102015.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\ast_20250514102016.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\ast_20250514104350.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\ast_20250514104354.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\ast_20250514104403.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\ast_20250514104415.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\ast_20250514104416.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\ast_20250514105005.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\ast_20250514105009.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\ast_20250514121909.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\ast_20250514121950.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\ast_20250514123729.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\ast_20250514123735.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\ast_20250514123739.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\ast_20250514123749.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\ast_20250514123803.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\ast_20250514123809.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\ast_20250514123942.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\ast_20250514123948.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\ast_20250514124123.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\ast_20250514124130.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\ast_20250514124132.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\ast_20250514135056.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\ast_20250514135102.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\ast_20250514135103.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\ast_20250514135109.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\ast_20250514135201.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\ast_20250514135209.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\ast_20250514135225.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\ast_20250514135357.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\ast_20250514141233.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\ast_20250514141302.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\ast_20250514141846.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\ast_20250514141848.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\ast_20250514141855.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\ast_20250514141924.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\ast_20250514141925.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\ast_20250514141935.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\ast_20250514142010.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\ast_20250514142024.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\ast_20250514142053.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\ast_20250514142118.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\ast_20250514142120.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\ast_20250514142140.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\ast_20250514142144.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\ast_20250514142153.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\ast_20250514142159.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\ast_20250514142247.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\ast_20250514143946.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\ast_20250514144001.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\ast_20250514144004.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\ast_20250514144103.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\ast_20250514145154.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\ast_20250514145215.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\ast_20250514145556.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\ast_20250514145613.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\ast_20250514145633.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\ast_20250514220410.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\ast_20250514220412.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\ast_20250514220444.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\ast_20250514220450.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\ast_20250514220717.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\ast_20250514222302.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\ast_20250514222304.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\ast_20250514222312.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\ast_20250514222322.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\ast_20250514222332.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\ast_20250514222345.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\ast_20250514222421.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\ast_20250514224838.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\defer_stmt_20250513092919.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\defer_stmt_20250513092930.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\defer_stmt_20250513092932.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\expr_20250512231113.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\expr_20250512231117.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\expr_20250512231119.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\expr_20250513042615.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\expr_20250513042619.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\match_stmt_20250511014816.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\match_stmt_20250511014821.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\match_stmt_20250511014822.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\match_stmt_20250511014823.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\match_stmt_20250511014831.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\match_stmt_20250511014836.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\match_stmt_20250511014837.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\match_stmt_20250512180100.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\match_stmt_20250512180101.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\match_stmt_20250512180122.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\match_stmt_20250512180123.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\match_stmt_20250513042638.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\match_stmt_20250513042639.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\match_stmt_20250513042646.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\match_stmt_20250513042930.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\match_stmt_20250513042931.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\match_stmt_20250513042947.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\match_stmt_20250513042953.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\match_stmt_20250513042954.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\match_stmt_20250514150026.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\option_result_expr_20250511014749.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\option_result_expr_20250511014754.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\option_result_expr_20250511014755.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\option_result_expr_20250511014904.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\option_result_expr_20250511014910.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\option_result_expr_20250511014911.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\option_result_expr_20250511014912.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\option_result_expr_20250514151502.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\option_result_expr_20250514151506.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\option_result_expr_20250514151542.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\option_result_expr_20250514151543.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\property_20250513092950.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\property_20250513093008.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\stmt_20250512231053.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\stmt_20250512231103.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\stmt_20250512231104.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\stmt_20250513042604.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\stmt_20250513042609.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\types_20250512231234.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\types_20250512231241.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\types_20250512231256.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\types_20250512231918.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\types_20250512232353.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\types_20250512232357.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\types_20250513042145.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\types_20250514095902.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\types_20250514095909.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\types_20250514214904.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\types_20250514214922.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\types_20250514214925.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\types_20250514214935.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\types_20250514214949.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\types_20250514215058.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\types_20250514220056.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\types_20250514220058.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\types_20250514220516.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\types_20250514220717.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\visitor_20250511014851.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\visitor_20250511014855.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\visitor_20250511014856.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\visitor_20250511014857.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\visitor_20250512130456.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\visitor_20250512130457.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\visitor_20250512180148.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\visitor_20250512180150.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\visitor_20250514144927.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\visitor_20250514144953.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\visitor_20250514145110.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\visitor_20250514145116.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\visitor_20250514151439.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ast\visitor_20250514151446.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250421121706.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250429230822.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250429230823.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250429230849.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250429230851.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250511010916.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250511010921.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250511012317.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250511012729.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250511012730.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250511012732.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250511012805.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250511012808.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250511012818.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250511012932.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250511013913.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250511013935.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250511013950.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250511014018.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250511014019.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250511014020.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250511014021.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250511014022.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250511014023.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250511014024.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250511014026.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250511014031.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250511014034.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250511014044.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250511014046.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250511014100.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250511015458.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250511015508.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250511015704.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250511015705.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250511015706.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250511015708.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250511015710.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250511015711.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250511015712.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250511015714.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250511015716.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250511015718.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250511015722.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250511015743.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250511145522.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250511145657.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250511145928.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250511145929.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250511145938.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250511150001.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250511150004.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250511150042.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250511150046.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250511150336.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250511150417.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250511150617.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250511150618.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250511150646.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250511150717.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250511150720.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250511150737.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250511150754.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250511151325.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250512112348.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250512112350.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250512112351.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250512112503.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250512112849.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250512112912.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250512113102.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250512121712.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250512121713.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250512121714.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250512121715.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250512121716.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250512121717.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250512121718.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250512121719.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250512121720.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250512121721.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250512121722.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250512121725.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250512121754.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250512121818.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250512121819.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250512121820.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250512121821.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250512121822.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250512121823.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250512121824.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250512121825.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250512121826.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250512121827.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250512121828.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250512121829.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250512121852.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250512121853.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250512121854.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250512121901.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250512121902.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250512121926.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250512121933.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250512122945.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250512122946.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250512122948.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250512122949.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250512122950.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250512123002.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250512123130.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250512123131.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250512135336.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250512135356.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250512150915.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250512151211.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250512151214.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250512151218.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250512181303.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250512181305.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250512181451.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250512181457.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250512181546.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250512181555.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250512181557.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250512181600.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250512181607.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250512181608.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250512181618.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250512181717.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250512181723.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250512181724.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250512181726.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250512181730.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250512181731.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250512181732.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250512181733.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250512181734.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250512181738.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250512181739.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250512181740.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250512181742.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250512181849.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250512181850.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250512181851.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250512181852.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250512181853.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250512181854.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250512181855.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250512181856.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250512181857.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250512181900.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250512181955.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250512181956.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250512181958.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250512182111.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250512182112.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250512182113.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250512182114.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250512182115.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250512182116.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250512182117.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250512182118.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250512182119.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250512182121.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250512182122.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250512182128.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250512182139.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250512182144.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250512182145.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250512182146.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250512182148.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250512182152.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513045541.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513045544.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513045558.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513045605.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513045726.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513045732.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513045919.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513045937.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513050017.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513050018.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513050019.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513050020.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513050025.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513050027.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513050030.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513050035.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513050139.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513050155.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513050200.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513050210.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513050224.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513050229.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513050239.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513050319.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513050400.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513050415.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513050417.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513050429.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513050439.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513050502.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513050509.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513050721.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513050722.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513050723.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513050724.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513050728.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513050754.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513051304.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513051305.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513051319.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513051525.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513051533.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513051536.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513051551.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513051556.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513051600.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513051640.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513051952.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513051953.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513052015.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513052019.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513052021.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513052026.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513052029.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513052033.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513052037.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513052042.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513052047.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513052202.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513052203.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513052222.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513052238.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513052316.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513060052.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513060057.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513060126.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513060131.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513060137.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513060138.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513060141.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513060147.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513060212.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513060226.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513060229.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513060230.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513060231.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513060232.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513060245.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513060304.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513060310.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513060311.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513060312.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513060316.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513060321.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513060328.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513060334.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513060337.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513060338.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513060339.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513060344.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513060347.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513060532.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513060539.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513060609.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513060617.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513060619.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513060620.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513060621.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513060623.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513060633.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513060640.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513060657.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513060658.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513060700.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513060720.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513062431.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513062432.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513062435.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513062438.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513062439.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513062442.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513062451.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513062453.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513062455.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513062457.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513062458.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513062501.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513062504.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513062507.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513062544.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513062551.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513062606.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513062612.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513062616.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513062618.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513062619.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513062620.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513062621.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513062623.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513062624.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513062626.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513062627.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513062628.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513062629.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513062630.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513062631.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513062632.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513062635.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513062640.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513062654.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513062705.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513062742.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513062749.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513062846.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513062854.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513062901.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513062912.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513062933.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513062942.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513065556.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513091053.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513091054.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513091055.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513091058.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513091100.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513091104.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513091107.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513091112.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513091118.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513091124.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513091126.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513091127.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513091129.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513091131.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513091138.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513091149.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513091151.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513091223.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513091224.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513091225.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513091304.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513092512.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513092514.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513092515.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513092517.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513092518.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513092519.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513092521.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513092522.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513092523.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513092524.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513092527.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513092529.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513092531.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513092532.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513092534.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513092538.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513092539.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513092540.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513092551.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250513092603.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250514095951.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250514095957.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250514095958.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250514100006.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250514100010.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250514100016.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250514100031.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250514100034.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250514100048.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250514100105.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250514100933.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250514100937.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250514100947.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250514100953.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250514100954.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250514101002.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250514101004.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250514101014.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250514101019.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250514101037.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250514101038.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250514101051.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250514101059.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250514101100.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250514101105.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250514101112.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250514101120.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250514101121.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250514101235.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250514101329.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250514101330.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250514101357.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250514101807.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250514101808.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250514101828.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250514101829.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250514101943.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250514101944.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250514101945.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250514101953.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250514102552.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250514102553.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250514102631.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250514105213.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250514105214.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250514105222.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250514105247.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250514105254.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250514105317.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250514105350.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250514105433.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250514105500.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250514105504.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250514105642.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250514122116.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250514124100.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250514124110.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250514124111.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250514135417.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250514135519.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250514135528.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250514135533.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250514135600.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250514135614.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250514135658.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250514141326.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250514141341.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250514141346.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250514141520.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250514142335.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250514142532.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250514142541.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250514142702.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250514142740.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250514142747.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250514142748.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250514144156.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250514144201.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250514144308.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250514162522.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250514162525.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250514162609.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250514163332.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250514163438.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250514163441.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250514163616.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250514163634.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250514164127.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250514164223.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250514164238.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250514164358.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250514164404.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250514164812.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250514164858.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250514165034.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250514165037.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250514165050.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250514165057.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250514165100.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250514165117.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250514165123.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250514165126.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250514165442.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250514165444.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250514170238.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250514170243.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250514170248.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250514170258.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250514183903.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250514183912.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250514183953.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250514183954.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250514184020.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\codegen\ir_generator_20250514205129.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\compiler"
  Delete "$INSTDIR\.history\tocin-compiler\src\compiler\compilation_context_20250415111606.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\compiler\compilation_context_20250421121509.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\compiler\compilation_context_20250512180532.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\compiler\compilation_context_20250512180534.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\compiler\compilation_context_20250512180608.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\compiler\compilation_context_20250512180609.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\compiler\compilation_context_20250512180616.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\compiler\compilation_context_20250512184024.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\compiler\compilation_context_20250512184025.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\compiler\compilation_context_20250512184026.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\compiler\compilation_context_20250512190513.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\compiler\compilation_context_20250512190516.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\compiler\compilation_context_20250512190518.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\compiler\compilation_context_20250513045022.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\compiler\compilation_context_20250513045023.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\compiler\compilation_context_20250514152128.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\compiler\compilation_context_20250514152135.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\compiler\compilation_context_20250514160217.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\compiler\compilation_context_20250514160239.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\compiler\compiler_20250511011434.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\compiler\compiler_20250511011436.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\compiler\compiler_20250511011439.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\compiler\compiler_20250511011512.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\compiler\compiler_20250511011517.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\compiler\compiler_20250511011523.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\compiler\compiler_20250513051030.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\compiler\compiler_20250513051031.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\compiler\compiler_20250513051110.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\compiler\compiler_20250514161344.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\compiler\compiler_20250514161346.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\compiler\compiler_20250514161855.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\compiler\compiler_20250514161909.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\compiler\compiler_20250514161925.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\compiler\compiler_20250514161944.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\compiler\compiler_20250514161954.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\compiler\compiler_20250514161955.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\compiler\compiler_20250514162800.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\compiler\compiler_20250514162801.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\compiler\compiler_20250514205606.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\compiler\compiler_20250514210014.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\compiler\compiler_20250514210015.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\error"
  Delete "$INSTDIR\.history\tocin-compiler\src\error\error_handler_20250429230251.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\error\error_handler_20250429230256.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\error\error_handler_20250429230326.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\error\error_handler_20250429230335.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\error\error_handler_20250512151322.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\error\error_handler_20250512151324.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\error\error_handler_20250512175256.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\error\error_handler_20250512175319.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\error\error_handler_20250512175327.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\error\error_handler_20250512175329.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\error\error_handler_20250512175553.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\error\error_handler_20250512175636.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\error\error_handler_20250512175637.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\error\error_handler_20250513044734.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\error\error_handler_20250513044753.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\error\error_handler_20250513044802.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\error\error_handler_20250513044813.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\error\error_handler_20250513044855.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\error\error_handler_20250513044902.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\error\error_handler_20250513044924.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\error\error_handler_20250514095817.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\error\error_handler_20250514095818.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\error\error_handler_20250514124200.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\error\error_handler_20250514124201.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\error\error_handler_20250514130143.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ffi"
  Delete "$INSTDIR\.history\tocin-compiler\src\ffi\ffi_javascript_20250413083555.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ffi\ffi_javascript_20250512160143.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ffi\ffi_javascript_20250512160146.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ffi\ffi_python_20250415090819.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ffi\ffi_python_20250512160134.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\ffi\ffi_python_20250512160136.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\lexer"
  Delete "$INSTDIR\.history\tocin-compiler\src\lexer\lexer_20250511011603.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\lexer\lexer_20250511011604.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\lexer\lexer_20250511011605.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\lexer\lexer_20250511011606.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\lexer\lexer_20250511011624.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\lexer\lexer_20250511100435.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\lexer\lexer_20250511100436.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\lexer\lexer_20250511100450.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\lexer\lexer_20250511100530.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\lexer\lexer_20250511100540.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\lexer\lexer_20250511100542.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\lexer\lexer_20250511100944.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\lexer\lexer_20250511101052.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\lexer\lexer_20250511102855.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\lexer\lexer_20250511102856.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\lexer\lexer_20250511132928.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\lexer\lexer_20250511132930.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\lexer\lexer_20250512131506.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\lexer\lexer_20250512131508.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\lexer\lexer_20250512231130.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\lexer\lexer_20250512231132.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\lexer\token_20250512130527.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\lexer\token_20250512130528.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\lexer\token_20250512231413.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\lexer\token_20250512231502.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\llvm_shim_20250512225235.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\llvm_shim_20250512225238.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\llvm_shim_20250512225239.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\llvm_shim_20250512225433.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\llvm_shim_20250512225434.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\llvm_shim_20250512230234.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\llvm_shim_20250512230316.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\llvm_shim_20250514162729.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\llvm_shim_20250514205524.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\llvm_shim_20250514210001.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\llvm_shim_20250514210002.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\main_20250511011537.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\main_20250511011539.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\main_20250511011540.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\main_20250511011557.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\main_20250512154641.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\main_20250512154642.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\main_20250512154643.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\main_20250512154644.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\main_20250512154645.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\main_20250512154647.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\main_20250512155157.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\main_20250512155159.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\main_20250512155213.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\main_20250512155458.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\main_20250512155459.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\main_20250512155501.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\main_20250512155512.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\main_20250512155514.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\main_20250512155516.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\main_20250512155520.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\main_20250512155527.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\main_20250512155528.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\main_20250512155546.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\main_20250512155826.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\main_20250512155827.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\main_20250512155835.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\main_20250512155838.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\main_20250512155840.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\main_20250512155844.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\main_20250512160121.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\main_20250512160158.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\main_20250512160348.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\main_20250512160349.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\main_20250512160415.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\main_20250512163526.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\main_20250512163527.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\main_20250512163540.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\main_20250512165501.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\main_20250512165502.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\main_20250512165518.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\main_20250512170215.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\main_20250512170217.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\main_20250512170245.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\main_20250512225250.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\main_20250512225252.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\main_20250513042814.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\main_20250513042816.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\main_20250513042856.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\main_20250513042858.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\main_20250513042901.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\main_20250513043310.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\main_20250513043312.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\main_20250513043407.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\main_20250513043410.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\main_20250513043553.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\main_20250513043555.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\main_20250513043620.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\main_20250513043622.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\main_20250513044001.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\main_20250513044005.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\main_20250513044115.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\main_20250513044117.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\main_20250513044332.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\main_20250513044333.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\main_20250513044335.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\main_20250513044434.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\main_20250513044442.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\main_20250513044446.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\main_20250513044620.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\main_20250513044624.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\main_20250513044625.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\main_20250513044626.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\main_20250513044628.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\main_20250513044702.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\main_20250513044708.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\main_20250513044709.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\main_20250513044710.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\main_20250513045432.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\main_20250513045435.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\main_20250513050502.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\main_20250513050756.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\main_20250513050759.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\main_20250513051002.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\main_20250513051004.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\main_20250513051007.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\main_20250513101534.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\main_20250513101939.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\parser"
  Delete "$INSTDIR\.history\tocin-compiler\src\parser\parser_20250421121742.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\parser\parser_20250511011626.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\parser\parser_20250511011627.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\parser\parser_20250511011628.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\parser\parser_20250511011629.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\parser\parser_20250511011644.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\parser\parser_20250511133656.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\parser\parser_20250511133657.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\parser\parser_20250511133700.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\parser\parser_20250512130401.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\parser\parser_20250512130402.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\parser\parser_20250512130422.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\parser\parser_20250512130423.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\parser\parser_20250512135653.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\parser\parser_20250512135655.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\pch_20250511015205.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\pch_20250511015210.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\pch_20250511015212.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\runtime"
  Delete "$INSTDIR\.history\tocin-compiler\src\runtime\concurrency_20250511015926.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\runtime\concurrency_20250511015933.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\runtime\concurrency_20250511020021.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\runtime\concurrency_20250512180457.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\runtime\concurrency_20250512180507.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\runtime\concurrency_20250513043057.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\runtime\concurrency_20250513043231.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\runtime\concurrency_20250513043249.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\runtime\concurrency_20250513092856.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\runtime\concurrency_20250513092904.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\runtime\concurrency_20250513092905.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\runtime\concurrency_20250513092907.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\runtime\concurrency_20250513092911.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\runtime\concurrency_20250514095003.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\runtime\concurrency_20250514103747.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\runtime\concurrency_20250514103748.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\runtime\concurrency_20250514103801.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\runtime\concurrency_20250514103812.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\runtime\concurrency_20250514103813.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\runtime\concurrency_20250514103820.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\runtime\concurrency_20250514103827.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\runtime\concurrency_20250514103831.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\runtime\concurrency_20250514103836.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\runtime\concurrency_20250514103844.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\runtime\concurrency_20250514103853.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\runtime\concurrency_20250514103854.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\runtime\concurrency_20250514103901.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\runtime\concurrency_20250514104324.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\runtime\concurrency_20250514104325.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\runtime\concurrency_20250514104327.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\runtime\concurrency_20250514104341.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\runtime\concurrency_20250514105050.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\runtime\concurrency_20250514105051.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\runtime\concurrency_20250514105052.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\runtime\concurrency_20250514105056.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\runtime\concurrency_20250514124232.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\runtime\concurrency_20250514124233.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\runtime\concurrency_20250514124245.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\runtime\concurrency_20250514124254.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\runtime\concurrency_20250514130143.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\runtime\concurrency_20250514135303.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\runtime\concurrency_20250514135357.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\runtime\concurrency_20250514143130.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\runtime\concurrency_20250514143217.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\runtime\concurrency_20250514143227.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\runtime\concurrency_20250514143238.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\runtime\concurrency_20250514143251.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\runtime\concurrency_20250514143259.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\runtime\concurrency_20250514143333.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\runtime\concurrency_20250514143425.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\runtime\concurrency_20250514143458.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\runtime\concurrency_20250514143507.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\runtime\concurrency_20250514143510.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\runtime\concurrency_20250514143520.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\runtime\concurrency_20250514143610.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\runtime\concurrency_20250514143614.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\runtime\concurrency_20250514144004.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\runtime\concurrency_20250514144120.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\runtime\linq_20250513100835.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\runtime\linq_20250513100841.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\runtime\linq_20250513100846.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\runtime\native_functions_20250429230603.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\runtime\native_functions_20250429230622.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\runtime\native_functions_20250429230758.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\runtime\native_functions_20250429230759.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\runtime\native_functions_20250429230808.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\target_info_20250512230212.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\target_info_20250512230230.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\test_20250513060942.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\test_20250513060945.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\test_20250513060948.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\type"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\extension_functions_20250513100959.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\extension_functions_20250513101004.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\extension_functions_20250513101009.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\extension_functions_20250514102611.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\extension_functions_20250514102612.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\extension_functions_20250514102614.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\extension_functions_20250514102624.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\extension_functions_20250514102650.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\extension_functions_20250514102651.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\extension_functions_20250514103127.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\extension_functions_20250514103150.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\extension_functions_20250514103155.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\extension_functions_20250514103205.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\extension_functions_20250514103206.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\extension_functions_20250514103209.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\extension_functions_20250514122026.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\extension_functions_20250514122035.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\feature_integration_20250513101258.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\feature_integration_20250513101303.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\feature_integration_20250513101308.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\feature_integration_20250514130213.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\feature_integration_20250514130215.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\feature_integration_20250514135245.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\feature_integration_20250514135247.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\move_semantics_20250513101204.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\move_semantics_20250513101209.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\move_semantics_20250513101213.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\move_semantics_20250514102755.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\move_semantics_20250514102813.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\move_semantics_20250514102815.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\move_semantics_20250514103422.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\move_semantics_20250514103429.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\move_semantics_20250514103447.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\move_semantics_20250514103516.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\move_semantics_20250514103517.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\move_semantics_20250514103534.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\move_semantics_20250514103541.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\move_semantics_20250514103542.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\move_semantics_20250514103659.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\move_semantics_20250514124000.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\move_semantics_20250514124001.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\move_semantics_20250514124013.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\move_semantics_20250514124018.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\move_semantics_20250514124034.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\move_semantics_20250514124041.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\move_semantics_20250514130143.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\move_semantics_20250514135319.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\move_semantics_20250514135322.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\move_semantics_20250514135357.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\move_semantics_20250514143028.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\move_semantics_20250514143125.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\move_semantics_20250514144024.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\move_semantics_20250514220547.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\move_semantics_20250514220548.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\move_semantics_20250514220717.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\null_safety_20250513100922.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\null_safety_20250513100927.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\null_safety_20250513100931.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\null_safety_20250514095924.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\null_safety_20250514095925.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\null_safety_20250514095926.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\null_safety_20250514095930.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\null_safety_20250514100826.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\null_safety_20250514100830.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\option_result_types_20250511014717.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\option_result_types_20250511014721.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\option_result_types_20250511014722.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\option_result_types_20250511014732.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\option_result_types_20250511014737.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\option_result_types_20250511014738.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\option_result_types_20250514150858.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\option_result_types_20250514150922.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\option_result_types_20250514151334.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\option_result_types_20250514151355.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\option_result_types_20250514215006.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\option_result_types_20250514215007.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\option_result_types_20250514215111.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\option_result_types_20250514215112.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\option_result_types_20250514215219.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\option_result_types_20250514215237.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\option_result_types_20250514215646.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\option_result_types_20250514220724.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\option_result_types_20250514221129.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\ownership_20250513092550.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\ownership_20250513092613.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\ownership_20250513092615.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\ownership_20250513092617.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\result_option_20250513092732.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\result_option_20250513092753.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\result_option_20250513092756.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\result_option_20250513092808.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\result_option_20250514095841.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\result_option_20250514095842.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\result_option_20250514095843.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\result_option_20250514095847.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\result_option_20250514095850.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\result_option_20250514095900.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\result_option_20250514130239.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\result_option_20250514130240.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\result_option_20250514131625.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\result_option_20250514161823.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\result_option_20250514162007.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\traits_20250513101107.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\traits_20250513101113.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\traits_20250513101125.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\traits_20250514102705.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\traits_20250514102706.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\traits_20250514102707.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\traits_20250514102708.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\traits_20250514102709.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\traits_20250514102710.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\traits_20250514102714.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\traits_20250514102725.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\traits_20250514102726.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\traits_20250514103220.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\traits_20250514103223.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\traits_20250514103252.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\traits_20250514103255.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\traits_20250514103304.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\traits_20250514103314.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\traits_20250514103316.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\traits_20250514103336.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\traits_20250514103341.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\traits_20250514103401.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\traits_20250514103406.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\traits_20250514122051.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\traits_20250514122056.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\traits_20250514123829.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\traits_20250514123831.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\traits_20250514123843.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\traits_20250514123848.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\traits_20250514123850.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\traits_20250514123901.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\traits_20250514123911.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\traits_20250514123912.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\traits_20250514123920.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\traits_20250514123924.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\traits_20250514123925.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\traits_20250514123927.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\traits_20250514220646.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\traits_20250514220648.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\traits_20250514222449.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\traits_20250514222451.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\trait_20250511015753.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\trait_20250511015757.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\trait_20250511015854.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\type_checker_20250421114249.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\type_checker_20250421162202.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\type_checker_20250511011653.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\type_checker_20250511011656.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\type_checker_20250511011658.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\type_checker_20250511011700.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\type_checker_20250511011743.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\type_checker_20250511134044.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\type_checker_20250511134049.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\type_checker_20250511134056.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\type_checker_20250511134059.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\type_checker_20250511134127.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\type_checker_20250511134129.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\type_checker_20250511134130.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\type_checker_20250511134132.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\type_checker_20250511134133.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\type_checker_20250511134134.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\type_checker_20250511134135.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\type_checker_20250511134136.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\type_checker_20250511134137.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\type_checker_20250511134138.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\type_checker_20250511134139.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\type_checker_20250511134142.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\type_checker_20250511135206.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\type_checker_20250511135210.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\type_checker_20250511135223.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\type_checker_20250511135226.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\type_checker_20250511135242.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\type_checker_20250511135245.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\type_checker_20250511135252.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\type_checker_20250511135258.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\type_checker_20250511135342.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\type_checker_20250511135345.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\type_checker_20250511135643.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\type_checker_20250511135646.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\type_checker_20250511135737.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\type_checker_20250511135748.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\type_checker_20250512133429.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\type_checker_20250512133430.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\type_checker_20250512135056.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\type_checker_20250512135058.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\type_checker_20250512135149.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\type_checker_20250512135210.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\type_checker_20250512135211.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\type_checker_20250512135212.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\type_checker_20250512135214.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\type_checker_20250512180728.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\type_checker_20250512180730.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\type_checker_20250512180803.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\type_checker_20250512180804.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\type_checker_20250512180805.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\type_checker_20250512180806.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\type_checker_20250512180808.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\type_checker_20250512180811.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\type_checker_20250512231433.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\type_checker_20250512231440.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\type_checker_20250513042705.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\type_checker_20250513042707.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\type_checker_20250513042722.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\type_checker_20250513042723.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\type_checker_20250513042725.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\type_checker_20250513101603.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\type_checker_20250513101605.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\type_checker_20250513101620.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\type_checker_20250513101621.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\type_checker_20250513101647.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\type_checker_20250513101650.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\type_checker_20250514142816.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\type_checker_20250514142948.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\type_checker_20250514142959.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\type_checker_20250514143006.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\type_checker_20250514162622.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\type_checker_20250514162703.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\type_checker_20250514162706.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\type_checker_20250514205554.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\type_checker_20250514205557.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\type_checker_20250514211521.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\type_checker_20250514211526.h"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\type_checker_20250514225510.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\src\type\type_checker_20250514225514.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\stdlib"
  Delete "$INSTDIR\.history\tocin-compiler\stdlib\audio"
  Delete "$INSTDIR\.history\tocin-compiler\stdlib\audio\audio_20250512192000.to"
  Delete "$INSTDIR\.history\tocin-compiler\stdlib\audio\audio_20250512192014.to"
  Delete "$INSTDIR\.history\tocin-compiler\stdlib\audio\audio_20250512192015.to"
  Delete "$INSTDIR\.history\tocin-compiler\stdlib\data"
  Delete "$INSTDIR\.history\tocin-compiler\stdlib\data\algorithms_20250512201458.to"
  Delete "$INSTDIR\.history\tocin-compiler\stdlib\data\algorithms_20250512201509.to"
  Delete "$INSTDIR\.history\tocin-compiler\stdlib\data\algorithms_20250512201511.to"
  Delete "$INSTDIR\.history\tocin-compiler\stdlib\data\algorithms_20250512201529.to"
  Delete "$INSTDIR\.history\tocin-compiler\stdlib\data\algorithms_20250512201533.to"
  Delete "$INSTDIR\.history\tocin-compiler\stdlib\data\algorithms_20250512201534.to"
  Delete "$INSTDIR\.history\tocin-compiler\stdlib\data\structures_20250512200615.to"
  Delete "$INSTDIR\.history\tocin-compiler\stdlib\data\structures_20250512200619.to"
  Delete "$INSTDIR\.history\tocin-compiler\stdlib\data\structures_20250512200621.to"
  Delete "$INSTDIR\.history\tocin-compiler\stdlib\database"
  Delete "$INSTDIR\.history\tocin-compiler\stdlib\database\database_20250512191624.to"
  Delete "$INSTDIR\.history\tocin-compiler\stdlib\database\database_20250512191627.to"
  Delete "$INSTDIR\.history\tocin-compiler\stdlib\database\database_20250512191629.to"
  Delete "$INSTDIR\.history\tocin-compiler\stdlib\embedded"
  Delete "$INSTDIR\.history\tocin-compiler\stdlib\embedded\gpio_20250512184809.to"
  Delete "$INSTDIR\.history\tocin-compiler\stdlib\embedded\gpio_20250512184813.to"
  Delete "$INSTDIR\.history\tocin-compiler\stdlib\embedded\gpio_20250512184814.to"
  Delete "$INSTDIR\.history\tocin-compiler\stdlib\game"
  Delete "$INSTDIR\.history\tocin-compiler\stdlib\game\engine_20250512193557.to"
  Delete "$INSTDIR\.history\tocin-compiler\stdlib\game\engine_20250512193602.to"
  Delete "$INSTDIR\.history\tocin-compiler\stdlib\game\engine_20250512193604.to"
  Delete "$INSTDIR\.history\tocin-compiler\stdlib\game\graphics_20250512201732.to"
  Delete "$INSTDIR\.history\tocin-compiler\stdlib\game\graphics_20250512201734.to"
  Delete "$INSTDIR\.history\tocin-compiler\stdlib\game\shader_20250512200246.to"
  Delete "$INSTDIR\.history\tocin-compiler\stdlib\game\shader_20250512200300.to"
  Delete "$INSTDIR\.history\tocin-compiler\stdlib\game\shader_20250512200302.to"
  Delete "$INSTDIR\.history\tocin-compiler\stdlib\gui"
  Delete "$INSTDIR\.history\tocin-compiler\stdlib\gui\core_20250512193009.to"
  Delete "$INSTDIR\.history\tocin-compiler\stdlib\gui\core_20250512193018.to"
  Delete "$INSTDIR\.history\tocin-compiler\stdlib\gui\core_20250512193019.to"
  Delete "$INSTDIR\.history\tocin-compiler\stdlib\gui\widgets_20250512200048.to"
  Delete "$INSTDIR\.history\tocin-compiler\stdlib\gui\widgets_20250512200052.to"
  Delete "$INSTDIR\.history\tocin-compiler\stdlib\gui\widgets_20250512200053.to"
  Delete "$INSTDIR\.history\tocin-compiler\stdlib\gui\widgets_20250512200054.to"
  Delete "$INSTDIR\.history\tocin-compiler\stdlib\math"
  Delete "$INSTDIR\.history\tocin-compiler\stdlib\math\basic_20250512192628.to"
  Delete "$INSTDIR\.history\tocin-compiler\stdlib\math\basic_20250512192632.to"
  Delete "$INSTDIR\.history\tocin-compiler\stdlib\math\basic_20250512192634.to"
  Delete "$INSTDIR\.history\tocin-compiler\stdlib\math\differential_20250512195313.to"
  Delete "$INSTDIR\.history\tocin-compiler\stdlib\math\differential_20250512195321.to"
  Delete "$INSTDIR\.history\tocin-compiler\stdlib\math\differential_20250512195323.to"
  Delete "$INSTDIR\.history\tocin-compiler\stdlib\math\differential_20250512200926.to"
  Delete "$INSTDIR\.history\tocin-compiler\stdlib\math\differential_20250512200927.to"
  Delete "$INSTDIR\.history\tocin-compiler\stdlib\math\differential_20250512200928.to"
  Delete "$INSTDIR\.history\tocin-compiler\stdlib\math\geometry_20250512192814.to"
  Delete "$INSTDIR\.history\tocin-compiler\stdlib\math\geometry_20250512192824.to"
  Delete "$INSTDIR\.history\tocin-compiler\stdlib\math\geometry_20250512192826.to"
  Delete "$INSTDIR\.history\tocin-compiler\stdlib\math\linear_20250512184302.to"
  Delete "$INSTDIR\.history\tocin-compiler\stdlib\math\linear_20250512184309.to"
  Delete "$INSTDIR\.history\tocin-compiler\stdlib\math\linear_20250512184311.to"
  Delete "$INSTDIR\.history\tocin-compiler\stdlib\math\stats_20250512184412.to"
  Delete "$INSTDIR\.history\tocin-compiler\stdlib\math\stats_20250512184418.to"
  Delete "$INSTDIR\.history\tocin-compiler\stdlib\math\stats_20250512184420.to"
  Delete "$INSTDIR\.history\tocin-compiler\stdlib\math\stats_advanced_20250512195704.to"
  Delete "$INSTDIR\.history\tocin-compiler\stdlib\math\stats_advanced_20250512195710.to"
  Delete "$INSTDIR\.history\tocin-compiler\stdlib\math\stats_advanced_20250512195712.to"
  Delete "$INSTDIR\.history\tocin-compiler\stdlib\math\stats_advanced_20250512201243.to"
  Delete "$INSTDIR\.history\tocin-compiler\stdlib\math\stats_advanced_20250512201244.to"
  Delete "$INSTDIR\.history\tocin-compiler\stdlib\math\stats_advanced_20250512201245.to"
  Delete "$INSTDIR\.history\tocin-compiler\stdlib\math\stats_advanced_20250512201339.to"
  Delete "$INSTDIR\.history\tocin-compiler\stdlib\math\stats_advanced_20250512201346.to"
  Delete "$INSTDIR\.history\tocin-compiler\stdlib\ml"
  Delete "$INSTDIR\.history\tocin-compiler\stdlib\ml\computer_vision_20250512191102.to"
  Delete "$INSTDIR\.history\tocin-compiler\stdlib\ml\computer_vision_20250512191105.to"
  Delete "$INSTDIR\.history\tocin-compiler\stdlib\ml\computer_vision_20250512191106.to"
  Delete "$INSTDIR\.history\tocin-compiler\stdlib\ml\computer_vision_20250512191450.to"
  Delete "$INSTDIR\.history\tocin-compiler\stdlib\ml\deep_learning_20250512190708.to"
  Delete "$INSTDIR\.history\tocin-compiler\stdlib\ml\deep_learning_20250512190716.to"
  Delete "$INSTDIR\.history\tocin-compiler\stdlib\ml\deep_learning_20250512190718.to"
  Delete "$INSTDIR\.history\tocin-compiler\stdlib\ml\neural_network_20250512184142.to"
  Delete "$INSTDIR\.history\tocin-compiler\stdlib\ml\neural_network_20250512184145.to"
  Delete "$INSTDIR\.history\tocin-compiler\stdlib\ml\neural_network_20250512184146.to"
  Delete "$INSTDIR\.history\tocin-compiler\stdlib\net"
  Delete "$INSTDIR\.history\tocin-compiler\stdlib\net\advanced_20250512200741.to"
  Delete "$INSTDIR\.history\tocin-compiler\stdlib\net\advanced_20250512200745.to"
  Delete "$INSTDIR\.history\tocin-compiler\stdlib\net\advanced_20250512200746.to"
  Delete "$INSTDIR\.history\tocin-compiler\stdlib\pkg"
  Delete "$INSTDIR\.history\tocin-compiler\stdlib\pkg\manager_20250512193207.to"
  Delete "$INSTDIR\.history\tocin-compiler\stdlib\pkg\manager_20250512193220.to"
  Delete "$INSTDIR\.history\tocin-compiler\stdlib\pkg\manager_20250512193221.to"
  Delete "$INSTDIR\.history\tocin-compiler\stdlib\scripting"
  Delete "$INSTDIR\.history\tocin-compiler\stdlib\scripting\automation_20250512184931.to"
  Delete "$INSTDIR\.history\tocin-compiler\stdlib\scripting\automation_20250512184934.to"
  Delete "$INSTDIR\.history\tocin-compiler\stdlib\scripting\automation_20250512184935.to"
  Delete "$INSTDIR\.history\tocin-compiler\stdlib\web"
  Delete "$INSTDIR\.history\tocin-compiler\stdlib\web\http_20250512184529.to"
  Delete "$INSTDIR\.history\tocin-compiler\stdlib\web\http_20250512184533.to"
  Delete "$INSTDIR\.history\tocin-compiler\stdlib\web\http_20250512184535.to"
  Delete "$INSTDIR\.history\tocin-compiler\stdlib\web\websocket_20250512184715.to"
  Delete "$INSTDIR\.history\tocin-compiler\stdlib\web\websocket_20250512184718.to"
  Delete "$INSTDIR\.history\tocin-compiler\stdlib\web\websocket_20250512184719.to"
  Delete "$INSTDIR\.history\tocin-compiler\test_llvm_20250513065852.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\test_llvm_20250513065858.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\test_llvm_20250513065900.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\test_llvm_20250513065913.ps1"
  Delete "$INSTDIR\.history\tocin-compiler\test_llvm_20250513065918.ps1"
  Delete "$INSTDIR\.history\tocin-compiler\test_llvm_20250513065926.ps1"
  Delete "$INSTDIR\.history\tocin-compiler\tocin-compiler"
  Delete "$INSTDIR\.history\tocin-compiler\tocin-compiler\create_icon_20250512224958.py"
  Delete "$INSTDIR\.history\tocin-compiler\tocin-compiler\create_icon_20250512225001.py"
  Delete "$INSTDIR\.history\tocin-compiler\tocin-compiler\create_icon_20250512225003.py"
  Delete "$INSTDIR\.history\tocin-compiler\tocin-compiler\create_icon_20250512225153.py"
  Delete "$INSTDIR\.history\tocin-compiler\verify_llvm_20250513091240.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\verify_llvm_20250513091255.cpp"
  Delete "$INSTDIR\.history\tocin-compiler\verify_llvm_20250513091258.cpp"
  Delete "$INSTDIR\.history\vcpkg_20250512154703.json"
  Delete "$INSTDIR\.history\vcpkg_20250512154705.json"
  Delete "$INSTDIR\.history\vcpkg_20250512154707.json"
  Delete "$INSTDIR\.history\vcpkg_20250512155039.json"
  Delete "$INSTDIR\.idea"
  Delete "$INSTDIR\.idea\.gitignore"
  Delete "$INSTDIR\.idea\editor.xml"
  Delete "$INSTDIR\.idea\inspectionProfiles"
  Delete "$INSTDIR\.idea\inspectionProfiles\Project_Default.xml"
  Delete "$INSTDIR\.idea\misc.xml"
  Delete "$INSTDIR\.idea\modules.xml"
  Delete "$INSTDIR\.idea\tocin-compiler.iml"
  Delete "$INSTDIR\.idea\vcs.xml"
  Delete "$INSTDIR\.idea\workspace.xml"
  Delete "$INSTDIR\.vscode"
  Delete "$INSTDIR\.vscode\c_cpp_properties.json"
  Delete "$INSTDIR\.vscode\launch.json"
  Delete "$INSTDIR\.vscode\settings.json"
  Delete "$INSTDIR\benchmarks"
  Delete "$INSTDIR\benchmarks\benchmark_compile_large.to"
  Delete "$INSTDIR\benchmarks\benchmark_runtime_concurrency.to"
  Delete "$INSTDIR\benchmarks\benchmark_runtime_ffi.to"
  Delete "$INSTDIR\benchmarks\benchmark_runtime_linq.to"
  Delete "$INSTDIR\benchmarks\run_benchmarks.ps1"
  Delete "$INSTDIR\BENCHMARKS.md"
  Delete "$INSTDIR\build"
  Delete "$INSTDIR\build\CMakeCache.txt"
  Delete "$INSTDIR\build\CMakeFiles"
  Delete "$INSTDIR\build\CMakeFiles\4.0.3"
  Delete "$INSTDIR\build\CMakeFiles\4.0.3\CMakeCCompiler.cmake"
  Delete "$INSTDIR\build\CMakeFiles\4.0.3\CMakeCXXCompiler.cmake"
  Delete "$INSTDIR\build\CMakeFiles\4.0.3\CMakeDetermineCompilerABI_C.bin"
  Delete "$INSTDIR\build\CMakeFiles\4.0.3\CMakeDetermineCompilerABI_CXX.bin"
  Delete "$INSTDIR\build\CMakeFiles\4.0.3\CMakeRCCompiler.cmake"
  Delete "$INSTDIR\build\CMakeFiles\4.0.3\CMakeSystem.cmake"
  Delete "$INSTDIR\build\CMakeFiles\4.0.3\CompilerIdC"
  Delete "$INSTDIR\build\CMakeFiles\4.0.3\CompilerIdC\a.exe"
  Delete "$INSTDIR\build\CMakeFiles\4.0.3\CompilerIdC\CMakeCCompilerId.c"
  Delete "$INSTDIR\build\CMakeFiles\4.0.3\CompilerIdC\tmp"
  Delete "$INSTDIR\build\CMakeFiles\4.0.3\CompilerIdCXX"
  Delete "$INSTDIR\build\CMakeFiles\4.0.3\CompilerIdCXX\a.exe"
  Delete "$INSTDIR\build\CMakeFiles\4.0.3\CompilerIdCXX\CMakeCXXCompilerId.cpp"
  Delete "$INSTDIR\build\CMakeFiles\4.0.3\CompilerIdCXX\tmp"
  Delete "$INSTDIR\build\CMakeFiles\AArch64TargetParserTableGen.dir"
  Delete "$INSTDIR\build\CMakeFiles\AArch64TargetParserTableGen.dir\build.make"
  Delete "$INSTDIR\build\CMakeFiles\AArch64TargetParserTableGen.dir\cmake_clean.cmake"
  Delete "$INSTDIR\build\CMakeFiles\AArch64TargetParserTableGen.dir\compiler_depend.make"
  Delete "$INSTDIR\build\CMakeFiles\AArch64TargetParserTableGen.dir\compiler_depend.ts"
  Delete "$INSTDIR\build\CMakeFiles\AArch64TargetParserTableGen.dir\DependInfo.cmake"
  Delete "$INSTDIR\build\CMakeFiles\AArch64TargetParserTableGen.dir\progress.make"
  Delete "$INSTDIR\build\CMakeFiles\acc_gen.dir"
  Delete "$INSTDIR\build\CMakeFiles\acc_gen.dir\build.make"
  Delete "$INSTDIR\build\CMakeFiles\acc_gen.dir\cmake_clean.cmake"
  Delete "$INSTDIR\build\CMakeFiles\acc_gen.dir\compiler_depend.make"
  Delete "$INSTDIR\build\CMakeFiles\acc_gen.dir\compiler_depend.ts"
  Delete "$INSTDIR\build\CMakeFiles\acc_gen.dir\DependInfo.cmake"
  Delete "$INSTDIR\build\CMakeFiles\acc_gen.dir\progress.make"
  Delete "$INSTDIR\build\CMakeFiles\ARMTargetParserTableGen.dir"
  Delete "$INSTDIR\build\CMakeFiles\ARMTargetParserTableGen.dir\build.make"
  Delete "$INSTDIR\build\CMakeFiles\ARMTargetParserTableGen.dir\cmake_clean.cmake"
  Delete "$INSTDIR\build\CMakeFiles\ARMTargetParserTableGen.dir\compiler_depend.make"
  Delete "$INSTDIR\build\CMakeFiles\ARMTargetParserTableGen.dir\compiler_depend.ts"
  Delete "$INSTDIR\build\CMakeFiles\ARMTargetParserTableGen.dir\DependInfo.cmake"
  Delete "$INSTDIR\build\CMakeFiles\ARMTargetParserTableGen.dir\progress.make"
  Delete "$INSTDIR\build\CMakeFiles\cmake.check_cache"
  Delete "$INSTDIR\build\CMakeFiles\CMakeConfigureLog.yaml"
  Delete "$INSTDIR\build\CMakeFiles\CMakeDirectoryInformation.cmake"
  Delete "$INSTDIR\build\CMakeFiles\CMakeRuleHashes.txt"
  Delete "$INSTDIR\build\CMakeFiles\CMakeScratch"
  Delete "$INSTDIR\build\CMakeFiles\CMakeTmp"
  Delete "$INSTDIR\build\CMakeFiles\InstallScripts.json"
  Delete "$INSTDIR\build\CMakeFiles\intrinsics_gen.dir"
  Delete "$INSTDIR\build\CMakeFiles\intrinsics_gen.dir\build.make"
  Delete "$INSTDIR\build\CMakeFiles\intrinsics_gen.dir\cmake_clean.cmake"
  Delete "$INSTDIR\build\CMakeFiles\intrinsics_gen.dir\compiler_depend.make"
  Delete "$INSTDIR\build\CMakeFiles\intrinsics_gen.dir\compiler_depend.ts"
  Delete "$INSTDIR\build\CMakeFiles\intrinsics_gen.dir\DependInfo.cmake"
  Delete "$INSTDIR\build\CMakeFiles\intrinsics_gen.dir\progress.make"
  Delete "$INSTDIR\build\CMakeFiles\ir_generator_direct.dir"
  Delete "$INSTDIR\build\CMakeFiles\ir_generator_direct.dir\build.make"
  Delete "$INSTDIR\build\CMakeFiles\ir_generator_direct.dir\cmake_clean.cmake"
  Delete "$INSTDIR\build\CMakeFiles\ir_generator_direct.dir\compiler_depend.make"
  Delete "$INSTDIR\build\CMakeFiles\ir_generator_direct.dir\compiler_depend.ts"
  Delete "$INSTDIR\build\CMakeFiles\ir_generator_direct.dir\DependInfo.cmake"
  Delete "$INSTDIR\build\CMakeFiles\ir_generator_direct.dir\progress.make"
  Delete "$INSTDIR\build\CMakeFiles\Makefile.cmake"
  Delete "$INSTDIR\build\CMakeFiles\Makefile2"
  Delete "$INSTDIR\build\CMakeFiles\omp_gen.dir"
  Delete "$INSTDIR\build\CMakeFiles\omp_gen.dir\build.make"
  Delete "$INSTDIR\build\CMakeFiles\omp_gen.dir\cmake_clean.cmake"
  Delete "$INSTDIR\build\CMakeFiles\omp_gen.dir\compiler_depend.make"
  Delete "$INSTDIR\build\CMakeFiles\omp_gen.dir\compiler_depend.ts"
  Delete "$INSTDIR\build\CMakeFiles\omp_gen.dir\DependInfo.cmake"
  Delete "$INSTDIR\build\CMakeFiles\omp_gen.dir\progress.make"
  Delete "$INSTDIR\build\CMakeFiles\pkgRedirects"
  Delete "$INSTDIR\build\CMakeFiles\progress.marks"
  Delete "$INSTDIR\build\CMakeFiles\RISCVTargetParserTableGen.dir"
  Delete "$INSTDIR\build\CMakeFiles\RISCVTargetParserTableGen.dir\build.make"
  Delete "$INSTDIR\build\CMakeFiles\RISCVTargetParserTableGen.dir\cmake_clean.cmake"
  Delete "$INSTDIR\build\CMakeFiles\RISCVTargetParserTableGen.dir\compiler_depend.make"
  Delete "$INSTDIR\build\CMakeFiles\RISCVTargetParserTableGen.dir\compiler_depend.ts"
  Delete "$INSTDIR\build\CMakeFiles\RISCVTargetParserTableGen.dir\DependInfo.cmake"
  Delete "$INSTDIR\build\CMakeFiles\RISCVTargetParserTableGen.dir\progress.make"
  Delete "$INSTDIR\build\CMakeFiles\TargetDirectories.txt"
  Delete "$INSTDIR\build\CMakeFiles\tocin.dir"
  Delete "$INSTDIR\build\CMakeFiles\tocin.dir\build.make"
  Delete "$INSTDIR\build\CMakeFiles\tocin.dir\cmake_clean.cmake"
  Delete "$INSTDIR\build\CMakeFiles\tocin.dir\compiler_depend.make"
  Delete "$INSTDIR\build\CMakeFiles\tocin.dir\compiler_depend.ts"
  Delete "$INSTDIR\build\CMakeFiles\tocin.dir\depend.make"
  Delete "$INSTDIR\build\CMakeFiles\tocin.dir\DependInfo.cmake"
  Delete "$INSTDIR\build\CMakeFiles\tocin.dir\flags.make"
  Delete "$INSTDIR\build\CMakeFiles\tocin.dir\includes_CXX.rsp"
  Delete "$INSTDIR\build\CMakeFiles\tocin.dir\link.txt"
  Delete "$INSTDIR\build\CMakeFiles\tocin.dir\linkLibs.rsp"
  Delete "$INSTDIR\build\CMakeFiles\tocin.dir\objects.a"
  Delete "$INSTDIR\build\CMakeFiles\tocin.dir\objects1.rsp"
  Delete "$INSTDIR\build\CMakeFiles\tocin.dir\progress.make"
  Delete "$INSTDIR\build\CMakeFiles\tocin.dir\src"
  Delete "$INSTDIR\build\CMakeFiles\tocin.dir\src\ast"
  Delete "$INSTDIR\build\CMakeFiles\tocin.dir\src\ast\ast.cpp.obj"
  Delete "$INSTDIR\build\CMakeFiles\tocin.dir\src\ast\ast.cpp.obj.d"
  Delete "$INSTDIR\build\CMakeFiles\tocin.dir\src\ast\match_stmt.cpp.obj"
  Delete "$INSTDIR\build\CMakeFiles\tocin.dir\src\ast\match_stmt.cpp.obj.d"
  Delete "$INSTDIR\build\CMakeFiles\tocin.dir\src\ast\option_result_expr.cpp.obj"
  Delete "$INSTDIR\build\CMakeFiles\tocin.dir\src\ast\option_result_expr.cpp.obj.d"
  Delete "$INSTDIR\build\CMakeFiles\tocin.dir\src\codegen"
  Delete "$INSTDIR\build\CMakeFiles\tocin.dir\src\codegen\ir_generator.cpp.obj"
  Delete "$INSTDIR\build\CMakeFiles\tocin.dir\src\codegen\ir_generator.cpp.obj.d"
  Delete "$INSTDIR\build\CMakeFiles\tocin.dir\src\compiler"
  Delete "$INSTDIR\build\CMakeFiles\tocin.dir\src\compiler\compilation_context.cpp.obj"
  Delete "$INSTDIR\build\CMakeFiles\tocin.dir\src\compiler\compilation_context.cpp.obj.d"
  Delete "$INSTDIR\build\CMakeFiles\tocin.dir\src\compiler\compiler.cpp.obj"
  Delete "$INSTDIR\build\CMakeFiles\tocin.dir\src\compiler\compiler.cpp.obj.d"
  Delete "$INSTDIR\build\CMakeFiles\tocin.dir\src\compiler\macro_system.cpp.obj"
  Delete "$INSTDIR\build\CMakeFiles\tocin.dir\src\compiler\macro_system.cpp.obj.d"
  Delete "$INSTDIR\build\CMakeFiles\tocin.dir\src\compiler\stdlib.cpp.obj"
  Delete "$INSTDIR\build\CMakeFiles\tocin.dir\src\compiler\stdlib.cpp.obj.d"
  Delete "$INSTDIR\build\CMakeFiles\tocin.dir\src\error"
  Delete "$INSTDIR\build\CMakeFiles\tocin.dir\src\error\error_handler.cpp.obj"
  Delete "$INSTDIR\build\CMakeFiles\tocin.dir\src\error\error_handler.cpp.obj.d"
  Delete "$INSTDIR\build\CMakeFiles\tocin.dir\src\ffi"
  Delete "$INSTDIR\build\CMakeFiles\tocin.dir\src\ffi\ffi_cpp.cpp.obj"
  Delete "$INSTDIR\build\CMakeFiles\tocin.dir\src\ffi\ffi_cpp.cpp.obj.d"
  Delete "$INSTDIR\build\CMakeFiles\tocin.dir\src\ffi\ffi_javascript.cpp.obj"
  Delete "$INSTDIR\build\CMakeFiles\tocin.dir\src\ffi\ffi_javascript.cpp.obj.d"
  Delete "$INSTDIR\build\CMakeFiles\tocin.dir\src\ffi\ffi_python.cpp.obj"
  Delete "$INSTDIR\build\CMakeFiles\tocin.dir\src\ffi\ffi_python.cpp.obj.d"
  Delete "$INSTDIR\build\CMakeFiles\tocin.dir\src\ffi\ffi_value.cpp.obj"
  Delete "$INSTDIR\build\CMakeFiles\tocin.dir\src\ffi\ffi_value.cpp.obj.d"
  Delete "$INSTDIR\build\CMakeFiles\tocin.dir\src\lexer"
  Delete "$INSTDIR\build\CMakeFiles\tocin.dir\src\lexer\lexer.cpp.obj"
  Delete "$INSTDIR\build\CMakeFiles\tocin.dir\src\lexer\lexer.cpp.obj.d"
  Delete "$INSTDIR\build\CMakeFiles\tocin.dir\src\lexer\token.cpp.obj"
  Delete "$INSTDIR\build\CMakeFiles\tocin.dir\src\lexer\token.cpp.obj.d"
  Delete "$INSTDIR\build\CMakeFiles\tocin.dir\src\main.cpp.obj"
  Delete "$INSTDIR\build\CMakeFiles\tocin.dir\src\main.cpp.obj.d"
  Delete "$INSTDIR\build\CMakeFiles\tocin.dir\src\parser"
  Delete "$INSTDIR\build\CMakeFiles\tocin.dir\src\parser\parser.cpp.obj"
  Delete "$INSTDIR\build\CMakeFiles\tocin.dir\src\parser\parser.cpp.obj.d"
  Delete "$INSTDIR\build\CMakeFiles\tocin.dir\src\runtime"
  Delete "$INSTDIR\build\CMakeFiles\tocin.dir\src\runtime\native_functions.cpp.obj"
  Delete "$INSTDIR\build\CMakeFiles\tocin.dir\src\runtime\native_functions.cpp.obj.d"
  Delete "$INSTDIR\build\CMakeFiles\tocin.dir\src\type"
  Delete "$INSTDIR\build\CMakeFiles\tocin.dir\src\type\feature_integration.cpp.obj"
  Delete "$INSTDIR\build\CMakeFiles\tocin.dir\src\type\feature_integration.cpp.obj.d"
  Delete "$INSTDIR\build\CMakeFiles\tocin.dir\src\type\null_safety.cpp.obj"
  Delete "$INSTDIR\build\CMakeFiles\tocin.dir\src\type\null_safety.cpp.obj.d"
  Delete "$INSTDIR\build\CMakeFiles\tocin.dir\src\type\ownership.cpp.obj"
  Delete "$INSTDIR\build\CMakeFiles\tocin.dir\src\type\ownership.cpp.obj.d"
  Delete "$INSTDIR\build\CMakeFiles\tocin.dir\src\type\traits.cpp.obj"
  Delete "$INSTDIR\build\CMakeFiles\tocin.dir\src\type\traits.cpp.obj.d"
  Delete "$INSTDIR\build\CMakeFiles\tocin.dir\src\type\type_checker.cpp.obj"
  Delete "$INSTDIR\build\CMakeFiles\tocin.dir\src\type\type_checker.cpp.obj.d"
  Delete "$INSTDIR\build\CMakeFiles\uninstall.dir"
  Delete "$INSTDIR\build\CMakeFiles\uninstall.dir\build.make"
  Delete "$INSTDIR\build\CMakeFiles\uninstall.dir\cmake_clean.cmake"
  Delete "$INSTDIR\build\CMakeFiles\uninstall.dir\compiler_depend.make"
  Delete "$INSTDIR\build\CMakeFiles\uninstall.dir\compiler_depend.ts"
  Delete "$INSTDIR\build\CMakeFiles\uninstall.dir\DependInfo.cmake"
  Delete "$INSTDIR\build\CMakeFiles\uninstall.dir\progress.make"
  Delete "$INSTDIR\build\CMakeFiles\vt_gen.dir"
  Delete "$INSTDIR\build\CMakeFiles\vt_gen.dir\build.make"
  Delete "$INSTDIR\build\CMakeFiles\vt_gen.dir\cmake_clean.cmake"
  Delete "$INSTDIR\build\CMakeFiles\vt_gen.dir\compiler_depend.make"
  Delete "$INSTDIR\build\CMakeFiles\vt_gen.dir\compiler_depend.ts"
  Delete "$INSTDIR\build\CMakeFiles\vt_gen.dir\DependInfo.cmake"
  Delete "$INSTDIR\build\CMakeFiles\vt_gen.dir\progress.make"
  Delete "$INSTDIR\build\cmake_install.cmake"
  Delete "$INSTDIR\build\cmake_uninstall.cmake"
  Delete "$INSTDIR\build\CPackConfig.cmake"
  Delete "$INSTDIR\build\CPackSourceConfig.cmake"
  Delete "$INSTDIR\build\ir_generator.o"
  Delete "$INSTDIR\build\llvm_host_test.cpp"
  Delete "$INSTDIR\build\Makefile"
  Delete "$INSTDIR\build\tocin.exe"
  Delete "$INSTDIR\build_msys2.sh"
  Delete "$INSTDIR\build_standalone.bat"
  Delete "$INSTDIR\build_windows.ps1"
  Delete "$INSTDIR\CMakeLists.txt"
  Delete "$INSTDIR\CMakePresets.json"
  Delete "$INSTDIR\cmake_uninstall.cmake.in"
  Delete "$INSTDIR\CODE_OF_CONDUCT.md"
  Delete "$INSTDIR\compile_test.bat"
  Delete "$INSTDIR\CONTRIBUTING.md"
  Delete "$INSTDIR\docs"
  Delete "$INSTDIR\docs\01_Introduction.md"
  Delete "$INSTDIR\docs\02_Getting_Started.md"
  Delete "$INSTDIR\docs\03_Language_Basics.md"
  Delete "$INSTDIR\docs\04_Standard_Library.md"
  Delete "$INSTDIR\docs\05_Advanced_Topics.md"
  Delete "$INSTDIR\docs\BUILDING.md"
  Delete "$INSTDIR\docs\CONCURRENCY.md"
  Delete "$INSTDIR\docs\doxygen"
  Delete "$INSTDIR\docs\doxygen\custom.css"
  Delete "$INSTDIR\docs\ERROR_HANDLING.md"
  Delete "$INSTDIR\docs\FFI.md"
  Delete "$INSTDIR\docs\IMPLEMENTATION_STATUS.md"
  Delete "$INSTDIR\docs\LANGUAGE_FEATURES.md"
  Delete "$INSTDIR\docs\LINQ.md"
  Delete "$INSTDIR\docs\NULL_SAFETY.md"
  Delete "$INSTDIR\docs\OPTION_RESULT_TYPES.md"
  Delete "$INSTDIR\docs\PERFORMANCE.md"
  Delete "$INSTDIR\docs\TRAITS.md"
  Delete "$INSTDIR\Doxyfile.in"
  Delete "$INSTDIR\examples"
  Delete "$INSTDIR\examples\concurrency_demo.to"
  Delete "$INSTDIR\examples\game_demo.to"
  Delete "$INSTDIR\examples\gui_demo.to"
  Delete "$INSTDIR\examples\ml_demo.to"
  Delete "$INSTDIR\examples\traits_demo.to"
  Delete "$INSTDIR\examples\web_demo.to"
  Delete "$INSTDIR\flowchart.mermaid"
  Delete "$INSTDIR\installer"
  Delete "$INSTDIR\installer\build_installer.ps1"
  Delete "$INSTDIR\installer\build_installer.sh"
  Delete "$INSTDIR\installer\windows"
  Delete "$INSTDIR\installer\windows\installer.nsi"
  Delete "$INSTDIR\interpreter"
  Delete "$INSTDIR\interpreter\build_packages.bat"
  Delete "$INSTDIR\interpreter\build_packages.sh"
  Delete "$INSTDIR\interpreter\CMakeLists.txt"
  Delete "$INSTDIR\interpreter\include"
  Delete "$INSTDIR\interpreter\include\Builtins.h"
  Delete "$INSTDIR\interpreter\include\Environment.h"
  Delete "$INSTDIR\interpreter\include\Interpreter.h"
  Delete "$INSTDIR\interpreter\include\Runtime.h"
  Delete "$INSTDIR\interpreter\installer"
  Delete "$INSTDIR\interpreter\installer\linux"
  Delete "$INSTDIR\interpreter\installer\linux\debian"
  Delete "$INSTDIR\interpreter\installer\linux\debian\control"
  Delete "$INSTDIR\interpreter\installer\windows"
  Delete "$INSTDIR\interpreter\installer\windows\installer.nsi"
  Delete "$INSTDIR\interpreter\README.md"
  Delete "$INSTDIR\interpreter\resources"
  Delete "$INSTDIR\interpreter\resources\convert_icon.sh"
  Delete "$INSTDIR\interpreter\resources\icon.ico"
  Delete "$INSTDIR\interpreter\resources\icon.svg"
  Delete "$INSTDIR\interpreter\run_msys.bat"
  Delete "$INSTDIR\interpreter\setup_msys.bat"
  Delete "$INSTDIR\interpreter\src"
  Delete "$INSTDIR\interpreter\src\Builtins.cpp"
  Delete "$INSTDIR\interpreter\src\Environment.cpp"
  Delete "$INSTDIR\interpreter\src\Interpreter.cpp"
  Delete "$INSTDIR\interpreter\src\Repl.cpp"
  Delete "$INSTDIR\interpreter\src\Runtime.cpp"
  Delete "$INSTDIR\LICENSE"
  Delete "$INSTDIR\QUICK_START.md"
  Delete "$INSTDIR\README.md"
  Delete "$INSTDIR\src"
  Delete "$INSTDIR\src\ast"
  Delete "$INSTDIR\src\ast\ast.cpp"
  Delete "$INSTDIR\src\ast\ast.h"
  Delete "$INSTDIR\src\ast\ast_node.h"
  Delete "$INSTDIR\src\ast\defer_stmt.h"
  Delete "$INSTDIR\src\ast\expr.h"
  Delete "$INSTDIR\src\ast\match_stmt.cpp"
  Delete "$INSTDIR\src\ast\match_stmt.h"
  Delete "$INSTDIR\src\ast\option_result_expr.cpp"
  Delete "$INSTDIR\src\ast\option_result_expr.h"
  Delete "$INSTDIR\src\ast\property.h"
  Delete "$INSTDIR\src\ast\stmt.h"
  Delete "$INSTDIR\src\ast\types.h"
  Delete "$INSTDIR\src\ast\visitor.h"
  Delete "$INSTDIR\src\codegen"
  Delete "$INSTDIR\src\codegen\ir_generator.cpp"
  Delete "$INSTDIR\src\codegen\ir_generator.h"
  Delete "$INSTDIR\src\compiler"
  Delete "$INSTDIR\src\compiler\compilation_context.cpp"
  Delete "$INSTDIR\src\compiler\compilation_context.h"
  Delete "$INSTDIR\src\compiler\compiler.cpp"
  Delete "$INSTDIR\src\compiler\compiler.h"
  Delete "$INSTDIR\src\compiler\macro_system.cpp"
  Delete "$INSTDIR\src\compiler\macro_system.h"
  Delete "$INSTDIR\src\compiler\stdlib.cpp"
  Delete "$INSTDIR\src\compiler\tocin_stdlib.h"
  Delete "$INSTDIR\src\debugger"
  Delete "$INSTDIR\src\debugger\debugger.h"
  Delete "$INSTDIR\src\error"
  Delete "$INSTDIR\src\error\error_handler.cpp"
  Delete "$INSTDIR\src\error\error_handler.h"
  Delete "$INSTDIR\src\ffi"
  Delete "$INSTDIR\src\ffi\ffi_cpp.cpp"
  Delete "$INSTDIR\src\ffi\ffi_cpp.h"
  Delete "$INSTDIR\src\ffi\ffi_interface.h"
  Delete "$INSTDIR\src\ffi\ffi_javascript.cpp"
  Delete "$INSTDIR\src\ffi\ffi_javascript.h"
  Delete "$INSTDIR\src\ffi\ffi_python.cpp"
  Delete "$INSTDIR\src\ffi\ffi_python.h"
  Delete "$INSTDIR\src\ffi\ffi_value.cpp"
  Delete "$INSTDIR\src\ffi\ffi_value.h"
  Delete "$INSTDIR\src\lexer"
  Delete "$INSTDIR\src\lexer\lexer.cpp"
  Delete "$INSTDIR\src\lexer\lexer.h"
  Delete "$INSTDIR\src\lexer\token.cpp"
  Delete "$INSTDIR\src\lexer\token.h"
  Delete "$INSTDIR\src\llvm_shim.h"
  Delete "$INSTDIR\src\main.cpp"
  Delete "$INSTDIR\src\package"
  Delete "$INSTDIR\src\package\package_manager.h"
  Delete "$INSTDIR\src\parser"
  Delete "$INSTDIR\src\parser\parser.cpp"
  Delete "$INSTDIR\src\parser\parser.h"
  Delete "$INSTDIR\src\pch.h"
  Delete "$INSTDIR\src\runtime"
  Delete "$INSTDIR\src\runtime\async_system.h"
  Delete "$INSTDIR\src\runtime\concurrency.h"
  Delete "$INSTDIR\src\runtime\linq.h"
  Delete "$INSTDIR\src\runtime\native_functions.cpp"
  Delete "$INSTDIR\src\runtime\native_functions.h"
  Delete "$INSTDIR\src\targets"
  Delete "$INSTDIR\src\targets\wasm_target.h"
  Delete "$INSTDIR\src\target_info.h"
  Delete "$INSTDIR\src\type"
  Delete "$INSTDIR\src\type\extension_functions.h"
  Delete "$INSTDIR\src\type\feature_integration.cpp"
  Delete "$INSTDIR\src\type\feature_integration.h"
  Delete "$INSTDIR\src\type\move_semantics.h"
  Delete "$INSTDIR\src\type\null_safety.cpp"
  Delete "$INSTDIR\src\type\null_safety.h"
  Delete "$INSTDIR\src\type\option_result_types.h"
  Delete "$INSTDIR\src\type\ownership.cpp"
  Delete "$INSTDIR\src\type\ownership.h"
  Delete "$INSTDIR\src\type\result.h"
  Delete "$INSTDIR\src\type\result_option.h"
  Delete "$INSTDIR\src\type\trait.h"
  Delete "$INSTDIR\src\type\traits.cpp"
  Delete "$INSTDIR\src\type\traits.h"
  Delete "$INSTDIR\src\type\type_checker.cpp"
  Delete "$INSTDIR\src\type\type_checker.h"
  Delete "$INSTDIR\stdlib"
  Delete "$INSTDIR\stdlib\audio"
  Delete "$INSTDIR\stdlib\audio\audio.to"
  Delete "$INSTDIR\stdlib\data"
  Delete "$INSTDIR\stdlib\data\algorithms.to"
  Delete "$INSTDIR\stdlib\data\structures.to"
  Delete "$INSTDIR\stdlib\database"
  Delete "$INSTDIR\stdlib\database\database.to"
  Delete "$INSTDIR\stdlib\embedded"
  Delete "$INSTDIR\stdlib\embedded\gpio.to"
  Delete "$INSTDIR\stdlib\game"
  Delete "$INSTDIR\stdlib\game\engine.to"
  Delete "$INSTDIR\stdlib\game\graphics.to"
  Delete "$INSTDIR\stdlib\game\shader.to"
  Delete "$INSTDIR\stdlib\gui"
  Delete "$INSTDIR\stdlib\gui\core.to"
  Delete "$INSTDIR\stdlib\gui\widgets.to"
  Delete "$INSTDIR\stdlib\math"
  Delete "$INSTDIR\stdlib\math\basic.to"
  Delete "$INSTDIR\stdlib\math\differential.to"
  Delete "$INSTDIR\stdlib\math\geometry.to"
  Delete "$INSTDIR\stdlib\math\linear.to"
  Delete "$INSTDIR\stdlib\math\stats.to"
  Delete "$INSTDIR\stdlib\math\stats_advanced.to"
  Delete "$INSTDIR\stdlib\ml"
  Delete "$INSTDIR\stdlib\ml\computer_vision.to"
  Delete "$INSTDIR\stdlib\ml\deep_learning.to"
  Delete "$INSTDIR\stdlib\ml\neural_network.to"
  Delete "$INSTDIR\stdlib\net"
  Delete "$INSTDIR\stdlib\net\advanced.to"
  Delete "$INSTDIR\stdlib\pkg"
  Delete "$INSTDIR\stdlib\pkg\manager.to"
  Delete "$INSTDIR\stdlib\scripting"
  Delete "$INSTDIR\stdlib\scripting\automation.to"
  Delete "$INSTDIR\stdlib\web"
  Delete "$INSTDIR\stdlib\web\http.to"
  Delete "$INSTDIR\stdlib\web\websocket.to"
  Delete "$INSTDIR\struct"
  Delete "$INSTDIR\tests"
  Delete "$INSTDIR\tests\debug_tokens.to"
  Delete "$INSTDIR\tests\test_compiler_integration.to"
  Delete "$INSTDIR\tests\test_complete_features.to"
  Delete "$INSTDIR\tests\test_concurrency_parser.to"
  Delete "$INSTDIR\tests\test_error_handling.to"
  Delete "$INSTDIR\tests\test_ffi.to"
  Delete "$INSTDIR\tests\test_linq.to"
  Delete "$INSTDIR\tests\test_null_safety.to"
  Delete "$INSTDIR\tests\test_repl.to"
  Delete "$INSTDIR\tests\test_stdlib_math.to"
  Delete "$INSTDIR\tests\test_stdlib_string.to"
  Delete "$INSTDIR\tests\test_traits.to"
  Delete "$INSTDIR\test_build.bat"
  Delete "$INSTDIR\test_ffi.cpp"
  Delete "$INSTDIR\Tocin_Logo.ico"
  Delete "$INSTDIR\_CPack_Packages"
  Delete "$INSTDIR\_CPack_Packages\win64-Source"
  Delete "$INSTDIR\_CPack_Packages\win64-Source\NSIS"
  Delete "$INSTDIR\_CPack_Packages\win64-Source\NSIS\Tocin-1.0.0-Source"

  RMDir "$INSTDIR\.cursor\commands"
  RMDir "$INSTDIR\.cursor"
  RMDir "$INSTDIR\.github\workflows"
  RMDir "$INSTDIR\.github"
  RMDir "$INSTDIR\.history\src\ast"
  RMDir "$INSTDIR\.history\src\codegen"
  RMDir "$INSTDIR\.history\src\error"
  RMDir "$INSTDIR\.history\src\ffi"
  RMDir "$INSTDIR\.history\src\runtime"
  RMDir "$INSTDIR\.history\src\type"
  RMDir "$INSTDIR\.history\src"
  RMDir "$INSTDIR\.history\stdlib\fs"
  RMDir "$INSTDIR\.history\stdlib\math"
  RMDir "$INSTDIR\.history\stdlib\ml"
  RMDir "$INSTDIR\.history\stdlib\net"
  RMDir "$INSTDIR\.history\stdlib\pkg"
  RMDir "$INSTDIR\.history\stdlib\web"
  RMDir "$INSTDIR\.history\stdlib"
  RMDir "$INSTDIR\.history\tocin-compiler\docs"
  RMDir "$INSTDIR\.history\tocin-compiler\examples"
  RMDir "$INSTDIR\.history\tocin-compiler\fixed_code"
  RMDir "$INSTDIR\.history\tocin-compiler\scripts"
  RMDir "$INSTDIR\.history\tocin-compiler\src\ast"
  RMDir "$INSTDIR\.history\tocin-compiler\src\codegen"
  RMDir "$INSTDIR\.history\tocin-compiler\src\compiler"
  RMDir "$INSTDIR\.history\tocin-compiler\src\error"
  RMDir "$INSTDIR\.history\tocin-compiler\src\ffi"
  RMDir "$INSTDIR\.history\tocin-compiler\src\lexer"
  RMDir "$INSTDIR\.history\tocin-compiler\src\parser"
  RMDir "$INSTDIR\.history\tocin-compiler\src\runtime"
  RMDir "$INSTDIR\.history\tocin-compiler\src\type"
  RMDir "$INSTDIR\.history\tocin-compiler\src"
  RMDir "$INSTDIR\.history\tocin-compiler\stdlib\audio"
  RMDir "$INSTDIR\.history\tocin-compiler\stdlib\data"
  RMDir "$INSTDIR\.history\tocin-compiler\stdlib\database"
  RMDir "$INSTDIR\.history\tocin-compiler\stdlib\embedded"
  RMDir "$INSTDIR\.history\tocin-compiler\stdlib\game"
  RMDir "$INSTDIR\.history\tocin-compiler\stdlib\gui"
  RMDir "$INSTDIR\.history\tocin-compiler\stdlib\math"
  RMDir "$INSTDIR\.history\tocin-compiler\stdlib\ml"
  RMDir "$INSTDIR\.history\tocin-compiler\stdlib\net"
  RMDir "$INSTDIR\.history\tocin-compiler\stdlib\pkg"
  RMDir "$INSTDIR\.history\tocin-compiler\stdlib\scripting"
  RMDir "$INSTDIR\.history\tocin-compiler\stdlib\web"
  RMDir "$INSTDIR\.history\tocin-compiler\stdlib"
  RMDir "$INSTDIR\.history\tocin-compiler\tocin-compiler"
  RMDir "$INSTDIR\.history\tocin-compiler"
  RMDir "$INSTDIR\.history"
  RMDir "$INSTDIR\.idea\inspectionProfiles"
  RMDir "$INSTDIR\.idea"
  RMDir "$INSTDIR\.vscode"
  RMDir "$INSTDIR\benchmarks"
  RMDir "$INSTDIR\build\CMakeFiles\4.0.3\CompilerIdC\tmp"
  RMDir "$INSTDIR\build\CMakeFiles\4.0.3\CompilerIdC"
  RMDir "$INSTDIR\build\CMakeFiles\4.0.3\CompilerIdCXX\tmp"
  RMDir "$INSTDIR\build\CMakeFiles\4.0.3\CompilerIdCXX"
  RMDir "$INSTDIR\build\CMakeFiles\4.0.3"
  RMDir "$INSTDIR\build\CMakeFiles\AArch64TargetParserTableGen.dir"
  RMDir "$INSTDIR\build\CMakeFiles\acc_gen.dir"
  RMDir "$INSTDIR\build\CMakeFiles\ARMTargetParserTableGen.dir"
  RMDir "$INSTDIR\build\CMakeFiles\CMakeScratch"
  RMDir "$INSTDIR\build\CMakeFiles\CMakeTmp"
  RMDir "$INSTDIR\build\CMakeFiles\intrinsics_gen.dir"
  RMDir "$INSTDIR\build\CMakeFiles\ir_generator_direct.dir"
  RMDir "$INSTDIR\build\CMakeFiles\omp_gen.dir"
  RMDir "$INSTDIR\build\CMakeFiles\pkgRedirects"
  RMDir "$INSTDIR\build\CMakeFiles\RISCVTargetParserTableGen.dir"
  RMDir "$INSTDIR\build\CMakeFiles\tocin.dir\src\ast"
  RMDir "$INSTDIR\build\CMakeFiles\tocin.dir\src\codegen"
  RMDir "$INSTDIR\build\CMakeFiles\tocin.dir\src\compiler"
  RMDir "$INSTDIR\build\CMakeFiles\tocin.dir\src\error"
  RMDir "$INSTDIR\build\CMakeFiles\tocin.dir\src\ffi"
  RMDir "$INSTDIR\build\CMakeFiles\tocin.dir\src\lexer"
  RMDir "$INSTDIR\build\CMakeFiles\tocin.dir\src\parser"
  RMDir "$INSTDIR\build\CMakeFiles\tocin.dir\src\runtime"
  RMDir "$INSTDIR\build\CMakeFiles\tocin.dir\src\type"
  RMDir "$INSTDIR\build\CMakeFiles\tocin.dir\src"
  RMDir "$INSTDIR\build\CMakeFiles\tocin.dir"
  RMDir "$INSTDIR\build\CMakeFiles\uninstall.dir"
  RMDir "$INSTDIR\build\CMakeFiles\vt_gen.dir"
  RMDir "$INSTDIR\build\CMakeFiles"
  RMDir "$INSTDIR\build"
  RMDir "$INSTDIR\docs\doxygen"
  RMDir "$INSTDIR\docs"
  RMDir "$INSTDIR\examples"
  RMDir "$INSTDIR\installer\windows"
  RMDir "$INSTDIR\installer"
  RMDir "$INSTDIR\interpreter\include"
  RMDir "$INSTDIR\interpreter\installer\linux\debian"
  RMDir "$INSTDIR\interpreter\installer\linux"
  RMDir "$INSTDIR\interpreter\installer\windows"
  RMDir "$INSTDIR\interpreter\installer"
  RMDir "$INSTDIR\interpreter\resources"
  RMDir "$INSTDIR\interpreter\src"
  RMDir "$INSTDIR\interpreter"
  RMDir "$INSTDIR\src\ast"
  RMDir "$INSTDIR\src\codegen"
  RMDir "$INSTDIR\src\compiler"
  RMDir "$INSTDIR\src\debugger"
  RMDir "$INSTDIR\src\error"
  RMDir "$INSTDIR\src\ffi"
  RMDir "$INSTDIR\src\lexer"
  RMDir "$INSTDIR\src\package"
  RMDir "$INSTDIR\src\parser"
  RMDir "$INSTDIR\src\runtime"
  RMDir "$INSTDIR\src\targets"
  RMDir "$INSTDIR\src\type"
  RMDir "$INSTDIR\src"
  RMDir "$INSTDIR\stdlib\audio"
  RMDir "$INSTDIR\stdlib\data"
  RMDir "$INSTDIR\stdlib\database"
  RMDir "$INSTDIR\stdlib\embedded"
  RMDir "$INSTDIR\stdlib\game"
  RMDir "$INSTDIR\stdlib\gui"
  RMDir "$INSTDIR\stdlib\math"
  RMDir "$INSTDIR\stdlib\ml"
  RMDir "$INSTDIR\stdlib\net"
  RMDir "$INSTDIR\stdlib\pkg"
  RMDir "$INSTDIR\stdlib\scripting"
  RMDir "$INSTDIR\stdlib\web"
  RMDir "$INSTDIR\stdlib"
  RMDir "$INSTDIR\tests"
  RMDir "$INSTDIR\_CPack_Packages\win64-Source\NSIS\Tocin-1.0.0-Source"
  RMDir "$INSTDIR\_CPack_Packages\win64-Source\NSIS"
  RMDir "$INSTDIR\_CPack_Packages\win64-Source"
  RMDir "$INSTDIR\_CPack_Packages"


!ifdef CPACK_NSIS_ADD_REMOVE
  ;Remove the add/remove program
  Delete "$INSTDIR\AddRemove.exe"
!endif

  ;Remove the uninstaller itself.
  Delete "$INSTDIR\Uninstall.exe"
  DeleteRegKey SHCTX "Software\Microsoft\Windows\CurrentVersion\Uninstall\Tocin Compiler"

  ;Remove the installation directory if it is empty.
  RMDir "$INSTDIR"

  ; Remove the registry entries.
  DeleteRegKey SHCTX "Software\Tocin Team\Tocin Compiler"

  ; Removes all optional components
  !insertmacro SectionList "RemoveSection_CPack"

  !insertmacro MUI_STARTMENU_GETFOLDER Application $MUI_TEMP

  Delete "$SMPROGRAMS\$MUI_TEMP\Uninstall.lnk"
  Delete "$SMPROGRAMS\$MUI_TEMP\Website.url"
  Delete "$SMPROGRAMS\$MUI_TEMP\GitHub.url"



  ;Delete empty start menu parent directories
  StrCpy $MUI_TEMP "$SMPROGRAMS\$MUI_TEMP"

  startMenuDeleteLoop:
    ClearErrors
    RMDir $MUI_TEMP
    GetFullPathName $MUI_TEMP "$MUI_TEMP\.."

    IfErrors startMenuDeleteLoopDone

    StrCmp "$MUI_TEMP" "$SMPROGRAMS" startMenuDeleteLoopDone startMenuDeleteLoop
  startMenuDeleteLoopDone:

  ; If the user changed the shortcut, then uninstall may not work. This should
  ; try to fix it.
  StrCpy $MUI_TEMP "$START_MENU"
  Delete "$SMPROGRAMS\$MUI_TEMP\Uninstall.lnk"


  ;Delete empty start menu parent directories
  StrCpy $MUI_TEMP "$SMPROGRAMS\$MUI_TEMP"

  secondStartMenuDeleteLoop:
    ClearErrors
    RMDir $MUI_TEMP
    GetFullPathName $MUI_TEMP "$MUI_TEMP\.."

    IfErrors secondStartMenuDeleteLoopDone

    StrCmp "$MUI_TEMP" "$SMPROGRAMS" secondStartMenuDeleteLoopDone secondStartMenuDeleteLoop
  secondStartMenuDeleteLoopDone:

  DeleteRegKey /ifempty SHCTX "Software\Tocin Team\Tocin Compiler"

  Push $INSTDIR\bin
  StrCmp $DO_NOT_ADD_TO_PATH_ "1" doNotRemoveFromPath 0
    Call un.RemoveFromPath
  doNotRemoveFromPath:
SectionEnd

;--------------------------------
; determine admin versus local install
; Is install for "AllUsers" or "JustMe"?
; Default to "JustMe" - set to "AllUsers" if admin or on Win9x
; This function is used for the very first "custom page" of the installer.
; This custom page does not show up visibly, but it executes prior to the
; first visible page and sets up $INSTDIR properly...
; Choose different default installation folder based on SV_ALLUSERS...
; "Program Files" for AllUsers, "My Documents" for JustMe...

Function .onInit
  StrCmp "ON" "ON" 0 inst

  ReadRegStr $0 HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Tocin Compiler" "UninstallString"
  StrCmp $0 "" inst

  MessageBox MB_YESNOCANCEL|MB_ICONEXCLAMATION \
  "Tocin Compiler is already installed. $\n$\nDo you want to uninstall the old version before installing the new one?" \
  /SD IDYES IDYES uninst IDNO inst
  Abort

;Run the uninstaller
uninst:
  ClearErrors
  # $0 should _always_ be quoted, however older versions of CMake did not
  # do this.  We'll conditionally remove the begin/end quotes.
  # Remove first char if quote
  StrCpy $2 $0 1 0      # copy first char
  StrCmp $2 "$\"" 0 +2  # if char is quote
  StrCpy $0 $0 "" 1     # remove first char
  # Remove last char if quote
  StrCpy $2 $0 1 -1     # copy last char
  StrCmp $2 "$\"" 0 +2  # if char is quote
  StrCpy $0 $0 -1       # remove last char

  StrLen $2 "\Uninstall.exe"
  StrCpy $3 $0 -$2 # remove "\Uninstall.exe" from UninstallString to get path
  ExecWait '"$0" /S _?=$3' ;Do not copy the uninstaller to a temp file

  IfErrors uninst_failed inst
uninst_failed:
  MessageBox MB_OK|MB_ICONSTOP "Uninstall failed."
  Abort


inst:
  ; Reads components status for registry
  !insertmacro SectionList "InitSection"

  ; check to see if /D has been used to change
  ; the install directory by comparing it to the
  ; install directory that is expected to be the
  ; default
  StrCpy $IS_DEFAULT_INSTALLDIR 0
  StrCmp "$INSTDIR" "$PROGRAMFILES64\Tocin Compiler" 0 +2
    StrCpy $IS_DEFAULT_INSTALLDIR 1

  StrCpy $SV_ALLUSERS "JustMe"
  ; if default install dir then change the default
  ; if it is installed for JustMe
  StrCmp "$IS_DEFAULT_INSTALLDIR" "1" 0 +2
    StrCpy $INSTDIR "$DOCUMENTS\Tocin Compiler"

  ClearErrors
  UserInfo::GetName
  IfErrors noLM
  Pop $0
  UserInfo::GetAccountType
  Pop $1
  StrCmp $1 "Admin" 0 +4
    SetShellVarContext all
    ;MessageBox MB_OK 'User "$0" is in the Admin group'
    StrCpy $SV_ALLUSERS "AllUsers"
    Goto done
  StrCmp $1 "Power" 0 +3
    SetShellVarContext all
    ;MessageBox MB_OK 'User "$0" is in the Power Users group'
    StrCpy $SV_ALLUSERS "AllUsers"
    Goto done

  noLM:
    StrCpy $SV_ALLUSERS "AllUsers"
    ;Get installation folder from registry if available

  done:
  StrCmp $SV_ALLUSERS "AllUsers" 0 +3
    StrCmp "$IS_DEFAULT_INSTALLDIR" "1" 0 +2
      StrCpy $INSTDIR "$PROGRAMFILES64\Tocin Compiler"

  StrCmp "ON" "ON" 0 noOptionsPage
    !insertmacro MUI_INSTALLOPTIONS_EXTRACT "NSIS.InstallOptions.ini"

  noOptionsPage:
FunctionEnd
