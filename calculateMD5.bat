@echo off
setlocal enabledelayedexpansion

set "SolutionDir=D:\Projects\HerculesAC"
set "files=%SolutionDir%\x64\Debug\HerculesAC.aes %SolutionDir%\Debug\GameMon.aes %SolutionDir%\x64\Debug\GameMon64.aes %SolutionDir%\x64\Debug\GameLauncher.exe %SolutionDir%\x64\Debug\cstrike.exe
set "outDir=%SolutionDir%\x64\Debug\hac.dat"


copy nul %outDir% > nul

echo [Game]>>%outDir%
echo starter=cstrike.exe>>%outDir%
echo Client=cstrike.exe>>%outDir%
echo GameMon=GameMon.aes>>%outDir%
echo GameMon64=GameMon64.aes>>%outDir%


echo [MD5]>>%outDir%

set /a count=0
for %%f in (%files%) do (
    set "filename=%%f"
    set "md5="
    
    for /f "usebackq delims=" %%i in (`certutil -hashfile "!filename!" MD5 ^| findstr /i /v "hash of file"`) do (
        set "md5=%%i"
        set "md5=!md5: =!"
    )
    
	echo hash!count!=!md5!>>%outDir%
    set /a count+=1
)

echo Count=!count!>>%outDir%

echo MD5 generated and saved into hac.dat file.