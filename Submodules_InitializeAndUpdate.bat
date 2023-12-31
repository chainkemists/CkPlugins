@echo off

echo ===============================================================================
echo Updating Submodules... please wait
echo -------------------------------------------------------------------------------
echo.

@echo on
git submodule add https://github.com/chainkemists/CkFoundation.git Plugins/CkFoundation
@echo off

@echo on
git submodule add https://github.com/chainkemists/CkGameplayDebugger.git Plugins/CkGameplayDebugger
@echo off

@echo on
git submodule add https://github.com/chainkemists/CkTests.git Plugins/CkTests
@echo off

@echo Initializing all submodules recrusively...
git submodule update --init --recursive

echo.
echo -------------------------------------------------------------------------------
echo Updating Submodules... DONE!
echo ===============================================================================
pause