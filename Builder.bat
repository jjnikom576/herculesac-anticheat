@echo off

call Project.bat

msbuild %GameLauncher% /p:Configuration="%buildConfig%" /p:Platform="x64"

msbuild %HerculesAC% /p:Configuration="%buildConfig%" /p:Platform="x64"

msbuild %GameMon% /p:Configuration="%buildConfig%" /p:Platform="x86"
msbuild %GameMon% /p:Configuration="%buildConfig%" /p:Platform="x64"

msbuild %GameProtect% /p:Configuration="%buildConfig%" /p:Platform="x86"
msbuild %GameProtect% /p:Configuration="%buildConfig%" /p:Platform="x64"

call vmp.bat
call calculateMD5.bat

pause