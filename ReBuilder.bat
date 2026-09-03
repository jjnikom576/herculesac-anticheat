@echo off

call Project.bat

msbuild %GameLauncher% /t:Rebuild /p:Configuration="%buildConfig%" /p:Platform="x64"

msbuild %HerculesAC% /t:Rebuild /p:Configuration="%buildConfig%" /p:Platform="x64"

msbuild %GameMon% /t:Rebuild /p:Configuration="%buildConfig%" /p:Platform="x86"
msbuild %GameMon% /t:Rebuild /p:Configuration="%buildConfig%" /p:Platform="x64"

msbuild %GameProtect% /t:Rebuild /p:Configuration="%buildConfig%" /p:Platform="x86"
msbuild %GameProtect% /t:Rebuild /p:Configuration="%buildConfig%" /p:Platform="x64"

call vmp.bat
call calculateMD5.bat

pause