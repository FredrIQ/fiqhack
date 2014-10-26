@echo on
@echo Reminders: have you copied over zlib, libpng, SDL2 to prebuilt?

rmdir /s ..\..\wix-build
mkdir ..\..\wix-build
cd ..\..\wix-build

rmdir /s ..\wix-install
mkdir ..\wix-install
perl ..\nicehack\aimake -i "C:\Program Files" --filelist=wix --destdir ..\wix-install ..\nicehack

cd ..\wix-install
copy ..\nicehack\dist\wix\installer.wxs nethack4.wxs
copy ..\nicehack\binary-copyright.rtf license.rtf

candle -arch x64 nethack4.wxs
light -ext WixUIExtension -dWixUILicenseRtf=license.rtf nethack4.wixobj
