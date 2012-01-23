Name "NitroHack"
OutFile "NitroHack-4.0.1.exe"
InstallDir "$PROGRAMFILES\NitroHack"

;--------------------------------

Page directory
Page instfiles

;--------------------------------

Section ""
  SetOutPath $INSTDIR
  File NitroHack\*
  FileOpen $0 $INSTDIR\record "a"
  FileClose $0
  AccessControl::GrantOnFile \
    "$INSTDIR\record" "(BU)" "GenericRead + GenericWrite"
  FileOpen $0 $INSTDIR\logfile "a"
  FileClose $0
  AccessControl::GrantOnFile \
    "$INSTDIR\logfile" "(BU)" "GenericRead + GenericWrite"

  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\NitroHack" "DisplayName" "NitroHack"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\NitroHack" "UninstallString" '"$INSTDIR\uninstall.exe"'
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\NitroHack" "NoModify" 1
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\NitroHack" "NoRepair" 1
  WriteUninstaller "uninstall.exe"
SectionEnd

Section "Desktop icon" SecDesktop
  SetOutPath $INSTDIR
  CreateShortcut "$DESKTOP\NitroHack.lnk" "$INSTDIR\NitroHack.exe" "" "$INSTDIR\NitroHack.exe" 0
SectionEnd

Section "Start Menu Shortcuts"
  SetOutPath $INSTDIR
  CreateShortCut "$SMPROGRAMS\NitroHack.lnk" "$INSTDIR\NitroHack.exe" "" "$INSTDIR\NitroHack.exe" 0
SectionEnd

Section "Uninstall"
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\NitroHack"
  DeleteRegKey HKLM SOFTWARE\NitroHack
  Delete $INSTDIR\*
  RMDir  $INSTDIR
  Delete $SMPROGRAMS\NitroHack.lnk
  Delete $DESKTOP\NitroHack.lnk
SectionEnd
