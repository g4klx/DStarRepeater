;NSIS Modern User Interface
;Repeater install script
;Written by Jonathan Naylor

;--------------------------------
;Include Modern UI

  !include "MUI2.nsh"

;--------------------------------
;Configuration

  ;General
  Name "DStarRepeater 20180911"
  OutFile "DStarRepeater64-20180911.exe"

  ;Folder selection page
  InstallDir "$PROGRAMFILES64\D-Star Repeater"
 
   ;Request application privileges for Windows Vista
  RequestExecutionLevel admin
 
;--------------------------------
;Interface Settings

  !define MUI_ABORTWARNING

;--------------------------------
;Pages

  !insertmacro MUI_PAGE_LICENSE "COPYING.txt"
  !insertmacro MUI_PAGE_DIRECTORY
  !insertmacro MUI_PAGE_INSTFILES
  !insertmacro MUI_UNPAGE_CONFIRM
  !insertmacro MUI_UNPAGE_INSTFILES
  
;--------------------------------
;Languages
 
  !insertmacro MUI_LANGUAGE "English"

;--------------------------------
;Installer Sections

Section "Repeater Program Files" SecProgram

  SetOutPath "$INSTDIR"

  File "x64\Release\DStarRepeater.exe"
  File "x64\Release\DStarRepeaterConfig.exe"
  File "..\portaudio\build\msvc\x64\Release\portaudio_x64.dll"
  File "C:\wxWidgets-3.0.4\lib\vc_x64_dll\wxbase30u_vc_x64_custom.dll"
  File "C:\wxWidgets-3.0.4\lib\vc_x64_dll\wxmsw30u_adv_vc_x64_custom.dll"
  File "C:\wxWidgets-3.0.4\lib\vc_x64_dll\wxmsw30u_core_vc_x64_custom.dll"
  File "WindowsUSB\xDVRPTR-32-64-2.inf"
  File "WindowsUSB\dvrptr_cdc.inf"
  File "WindowsUSB\gmsk.inf"
  File "WindowsUSB\gmsk.cat"
  File "Data\de_DE.ambe"
  File "Data\de_DE.indx"
  File "Data\dk_DK.ambe"
  File "Data\dk_DK.indx"
  File "Data\en_GB.ambe"
  File "Data\en_GB.indx"
  File "Data\en_US.ambe"
  File "Data\en_US.indx"
  File "Data\es_ES.ambe"
  File "Data\es_ES.indx"
  File "Data\fr_FR.ambe"
  File "Data\fr_FR.indx"
  File "Data\it_IT.ambe"
  File "Data\it_IT.indx"
  File "Data\no_NO.ambe"
  File "Data\no_NO.indx"
  File "Data\pl_PL.ambe"
  File "Data\pl_PL.indx"
  File "Data\se_SE.ambe"
  File "Data\se_SE.indx"
  File "COPYING.txt"
  File "CHANGES.txt"

  ;Create start menu entry
  CreateDirectory "$SMPROGRAMS\D-Star Repeater"
  CreateShortCut "$SMPROGRAMS\D-Star Repeater\D-Star Repeater.lnk"          "$INSTDIR\DStarRepeater.exe"
  CreateShortCut "$SMPROGRAMS\D-Star Repeater\D-Star Repeater Config.lnk"   "$INSTDIR\DStarRepeaterConfig.exe"
  CreateShortCut "$SMPROGRAMS\D-Star Repeater\Licence.lnk"                  "$INSTDIR\COPYING.txt"
  CreateShortCut "$SMPROGRAMS\D-Star Repeater\Changes.lnk"                  "$INSTDIR\CHANGES.txt"
  CreateShortCut "$SMPROGRAMS\D-Star Repeater\Uninstall.lnk"                "$INSTDIR\Uninstall.exe"

  ;Create uninstaller
  WriteUninstaller "$INSTDIR\Uninstall.exe"

SectionEnd

;--------------------------------
;Uninstaller Section

Section "Uninstall"

  Delete "$INSTDIR\*.*"
  RMDir  "$INSTDIR"

  Delete "$SMPROGRAMS\D-Star Repeater\*.*"
  RMDir  "$SMPROGRAMS\D-Star Repeater"

  DeleteRegKey /ifempty HKCU "Software\G4KLX\D-Star Repeater"

SectionEnd
