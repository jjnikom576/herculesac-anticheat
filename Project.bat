@echo off
set "REPO=%~dp0"
set "GameLauncher=%REPO%GameLauncher\GameLauncher.vcxproj"
set "HerculesAC=%REPO%HerculesAC\HerculesAC.vcxproj"
set "GameMon=%REPO%GameMon\GameMon.vcxproj"
set "GameProtect=%REPO%GameProtect\GameProtect.vcxproj"
set "HerculesKernel=%REPO%herculeskernel\herculeskernel.vcxproj"
set "buildConfig=Debug"

rem Kernel driver requires VS 2022 MSBuild (WDK 26100 ships DriverKit.Build.Tasks.17.0 only;
rem VS 2026 MSBuild 18 is incompatible with that task assembly).
set "MSBUILD_VS2022=C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe"