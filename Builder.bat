@echo off
setlocal

call "%~dp0Project.bat"

msbuild "%GameLauncher%" /p:Configuration="%buildConfig%" /p:Platform="x64"

msbuild "%HerculesAC%" /p:Configuration="%buildConfig%" /p:Platform="x64"

msbuild "%GameMon%" /p:Configuration="%buildConfig%" /p:Platform="x86"
msbuild "%GameMon%" /p:Configuration="%buildConfig%" /p:Platform="x64"

msbuild "%GameProtect%" /p:Configuration="%buildConfig%" /p:Platform="x86"
msbuild "%GameProtect%" /p:Configuration="%buildConfig%" /p:Platform="x64"

call "%~dp0vmp.bat"

powershell -ExecutionPolicy Bypass -File "%~dp0tools\gen-manifest.ps1" ^
    -OutDir "%~dp0x64\%buildConfig%" ^
    -GameDir "C:\Games\CS" ^
    -PrivateKey "%~dp0tools\keys\hac-dev.private.key"

pause