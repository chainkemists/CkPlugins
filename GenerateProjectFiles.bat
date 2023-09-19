@echo off

setlocal ENABLEEXTENSIONS
setlocal ENABLEDELAYEDEXPANSION

rem Query the registry for the engine directory
set REG_KEY="HKEY_CURRENT_USER\Software\Epic Games\Unreal Engine\Builds"
set REG_VALUE="CK-UE5"
FOR /F "usebackq tokens=3*" %%A IN (`REG QUERY %REG_KEY% /v %REG_VALUE%`) DO (
    set EngineDir=%%A %%B
)

rem Weird batch stuff, if the value ends with a space, it will be trimmed
set "len=!EngineDir!"
:loop
if "!len:~-1!"==" " (
    set "len=!len:~0,-1!"
    goto :loop
)

rem Set to the trimmed value
set "EngineDir=!len!"

if defined EngineDir (
    @echo Engine directory = %EngineDir%
) else (
    @echo Engine directory not found.
)

echo.
echo ===============================================================================
echo Generating Project files... please wait
echo -------------------------------------------------------------------------------
echo.

for %%f in (*.uproject) do (
    if "%%~xf"==".uproject" set PROJECT_FILE=%%f
)

if defined PROJECT_FILE (
    @echo.
    @echo UPROJECT found: %PROJECT_FILE%
    @echo.
) else (
    @echo uproject NOT found in %CD%
    goto :error
)

rem Ensure ShaderCompileWorker is built
call "%EngineDir%"\Engine\Build\BatchFiles\Build.bat ShaderCompileWorker Win64 Development || goto :error

rem Generate project files
call "%EngineDir%"\Engine\Build\BatchFiles\GenerateProjectFiles.bat -project="%~dp0\%PROJECT_FILE%" -engine -game || goto :error

echo.
echo -------------------------------------------------------------------------------
echo Generating Project files... DONE!
echo ===============================================================================

pause
exit /b 0

:error
echo Generating project files failed.
pause
exit /b 1