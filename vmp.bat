@echo off
set vmprotect="..\anticheat\VMProtect Ultimate v3.3.1 x32-x64 Build 1076 Retail Licensed-Released.By.Sound.Ret\x64\VMProtect_Con.exe"
set HerculesAC="%~dp0x64\Debug\HerculesAC.exe"
set GameMon="%~dp0Debug\GameMon.exe"
set GameMon64="%~dp0x64\Debug\GameMon64.exe"

%vmprotect% %HerculesAC%
%vmprotect% %GameMon%
%vmprotect% %GameMon64%
