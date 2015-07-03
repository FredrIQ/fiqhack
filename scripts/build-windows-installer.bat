@echo on
@echo Reminders: have you copied over zlib, libpng, SDL2 to prebuilt?

rem rmdir /s ..\..\wix-build
rem mkdir ..\..\wix-build
cd ..\..\wix-build

rmdir /s /q ..\wix-install
mkdir ..\wix-install
perl ..\nicehack\aimake -i "C:\Program Files" --filelist=wix --with=sourcecode --with=tilecompile --destdir ..\wix-install ..\nicehack

cd ..\wix-install

candle -arch x64 nethack4.wxs
light -ext WixUIExtension "-dWixUILicenseRtf=CSIDL_PROGRAM_FILES/NetHack 4/doc/license.rtf" -sice:ICE38 -sice:ICE43 -sice:ICE57 -sice:ICE64 -sice:ICE90 nethack4.wixobj
