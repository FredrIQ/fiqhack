Name "NetHack4"
OutFile "NetHack4-4.0.1.exe"
InstallDir "$PROGRAMFILES\NetHack4"

;--------------------------------

Page directory
Page instfiles

;--------------------------------

Section ""
  SetOutPath $INSTDIR
  File NetHack4\*
  FileOpen $0 $INSTDIR\record "a"
  FileClose $0
  AccessControl::GrantOnFile \
    "$INSTDIR\record" "(BU)" "GenericRead + GenericWrite"
  FileOpen $0 $INSTDIR\logfile "a"
  FileClose $0
  AccessControl::GrantOnFile \
    "$INSTDIR\logfile" "(BU)" "GenericRead + GenericWrite"

  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\NetHack4" "DisplayName" "NetHack4"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\NetHack4" "UninstallString" '"$INSTDIR\uninstall.exe"'
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\NetHack4" "NoModify" 1
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\NetHack4" "NoRepair" 1
  WriteUninstaller "uninstall.exe"
SectionEnd

Section "Desktop icon" SecDesktop
  SetOutPath $INSTDIR
  CreateShortcut "$DESKTOP\NetHack4.lnk" "$INSTDIR\NetHack4.exe" "" "$INSTDIR\NetHack4.exe" 0
SectionEnd

Section "Start Menu Shortcuts"
  SetOutPath $INSTDIR
  CreateShortCut "$SMPROGRAMS\NetHack4.lnk" "$INSTDIR\NetHack4.exe" "" "$INSTDIR\NetHack4.exe" 0
SectionEnd

Section "Uninstall"
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\NetHack4"
  DeleteRegKey HKLM SOFTWARE\NetHack4
  Delete $INSTDIR\*
  RMDir  $INSTDIR
  Delete $SMPROGRAMS\NetHack4.lnk
  Delete $DESKTOP\NetHack4.lnk
SectionEnd
