@echo off

echo ===============================================================================
echo Updating Submodules... please wait
echo -------------------------------------------------------------------------------
echo.

@echo on
git submodule add https://github.com/chainkemists/CkFoundation.git Plugins/CkFoundation
git submodule update --init --recursive
@echo off

echo.
echo -------------------------------------------------------------------------------
echo Updating Submodules... DONE!
echo ===============================================================================
pause