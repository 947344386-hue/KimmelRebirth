@echo off
setlocal enabledelayedexpansion
title KimmelRebirth Build

set UBT=C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.exe
set PROJECT=F:\UELibrary\KimmelRebirth\KimmelRebirth.uproject
set BUILD_BAT=C:\Program Files\Epic Games\UE_5.6\Engine\Build\BatchFiles\Build.bat

echo.
echo ============================================
echo   KimmelRebirth - Build Menu
echo ============================================
echo.
echo   [1] Fast incremental    (skip project gen, best when only .cpp changed)
echo   [2] Full rebuild        (project gen + full compile, use after adding files)
echo   [3] Quick project gen   (only regenerate .sln, no compile)
echo   [Q] Quit
echo.
set /p CHOICE="Pick [1/2/3/Q]: "

if /I "%CHOICE%"=="Q" exit /b 0
if "%CHOICE%"=="1" goto FAST
if "%CHOICE%"=="2" goto FULL
if "%CHOICE%"=="3" goto GENONLY
echo Invalid choice.
pause
exit /b 1

:GENONLY
echo.
echo [Project Gen Only] Regenerating .sln...
taskkill /F /IM UnrealEditor.exe >NUL 2>&1
taskkill /F /IM UnrealEditor-Cmd.exe >NUL 2>&1
timeout /t 1 /nobreak >NUL
"%UBT%" -projectfiles -project="%PROJECT%" -game -engine -progress
if %ERRORLEVEL% NEQ 0 (
    echo [X] FAILED.
    pause
    exit /b 1
)
echo [v] Done. Editor can now see new files.
pause
exit /b 0

:FAST
echo.
echo [Fast Incremental] Skipping project gen, compiling only changed modules...
taskkill /F /IM UnrealEditor.exe >NUL 2>&1
taskkill /F /IM UnrealEditor-Cmd.exe >NUL 2>&1
timeout /t 1 /nobreak >NUL
echo.
call "%BUILD_BAT%" KimmelRebirthEditor Win64 Development "%PROJECT%" -waitmutex
goto RESULT

:FULL
echo.
echo [Full Rebuild] Project gen + compile...
taskkill /F /IM UnrealEditor.exe >NUL 2>&1
taskkill /F /IM UnrealEditor-Cmd.exe >NUL 2>&1
timeout /t 1 /nobreak >NUL
echo.
echo [1/2] Project files...
"%UBT%" -projectfiles -project="%PROJECT%" -game -engine -progress
if %ERRORLEVEL% NEQ 0 (
    echo [X] Project gen FAILED.
    pause
    exit /b 1
)
echo.
echo [2/2] Compiling...
call "%BUILD_BAT%" KimmelRebirthEditor Win64 Development "%PROJECT%" -waitmutex
goto RESULT

:RESULT
if %ERRORLEVEL% NEQ 0 (
    echo.
    echo ============================================
    echo   COMPILE FAILED
    echo ============================================
    pause
    exit /b 1
)
echo.
echo ============================================
echo   COMPILE SUCCESS
echo ============================================
pause
