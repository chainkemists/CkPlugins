@echo off

echo ===============================================================================
echo Updating Marketplace Plugins Submodules... please wait
echo -------------------------------------------------------------------------------
echo.

@echo on
git submodule add https://github.com/chainkemists/AssetHistoryTracker.git Plugins/AssetHistoryTracker
@echo off

@echo on
git submodule add https://github.com/chainkemists/AutoSizeComments.git Plugins/AutoSizeComments
@echo off

@echo on
git submodule add https://github.com/chainkemists/BlueprintAssist.git Plugins/BlueprintAssist
@echo off

@echo on
git submodule add https://github.com/chainkemists/BlueprintGraphScreenshot.git Plugins/BlueprintGraphScreenshot
@echo off

@echo on
git submodule add https://github.com/chainkemists/BlueprintNodeTemplate.git Plugins/BlueprintNodeTemp
@echo off

@echo on
git submodule add https://github.com/chainkemists/EasyAssetCreator.git Plugins/EasyAssetCreator
@echo off

@echo on
git submodule add https://github.com/chainkemists/UEGitPlugin.git Plugins/GitSourceControl
@echo off

@echo on
git submodule add https://github.com/chainkemists/UEGitPlugin.git Plugins/GitSourceControl
@echo off

@echo on
git submodule add https://github.com/chainkemists/MDMetaDataEditor.git Plugins/MDMetaDataEditor
@echo off

@echo on
git submodule add https://github.com/chainkemists/NodeGraphAssistant.git Plugins/NodeGraphAssistant
@echo off

echo ===============================================================================
echo Updating CK Submodules... please wait
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