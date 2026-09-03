@echo off
setlocal

call "%~dp0Project.bat"

rem Kernel driver: must use VS 2022 MSBuild (see Project.bat for why).
"%MSBUILD_VS2022%" "%HerculesKernel%" /t:Rebuild /p:Configuration="%buildConfig%" /p:Platform="x64"

msbuild "%GameLauncher%" /t:Rebuild /p:Configuration="%buildConfig%" /p:Platform="x64"

msbuild "%HerculesAC%" /t:Rebuild /p:Configuration="%buildConfig%" /p:Platform="x64"

msbuild "%GameMon%" /t:Rebuild /p:Configuration="%buildConfig%" /p:Platform="x86"
msbuild "%GameMon%" /t:Rebuild /p:Configuration="%buildConfig%" /p:Platform="x64"

msbuild "%GameProtect%" /t:Rebuild /p:Configuration="%buildConfig%" /p:Platform="x86"
msbuild "%GameProtect%" /t:Rebuild /p:Configuration="%buildConfig%" /p:Platform="x64"

call "%~dp0vmp.bat"

powershell -ExecutionPolicy Bypass -File "%~dp0tools\gen-manifest.ps1" ^
    -OutDir "%~dp0x64\%buildConfig%" ^
    -GameDir "C:\Games\CS" ^
    -PrivateKey "%~dp0tools\keys\hac-dev.private.key"

pause