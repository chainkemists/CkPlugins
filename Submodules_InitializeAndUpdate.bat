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

@echo on
git submodule add https://github.com/chainkemists/WorldSpaceWidgets.git Plugins/WorldSpaceWidgets
@echo off

@echo on
git submodule add https://github.com/chainkemists/NumberRenderer.git Plugins/NumberRenderer
@echo off

@echo on
git submodule add https://github.com/chainkemists/ZomgEditorAddons.git Plugins/ZomgEditorAddons
@echo off

@echo on
git submodule add https://github.com/chainkemists/ZenMode.git Plugins/ZenMode
@echo off

@echo on
git submodule add https://github.com/chainkemists/DebugFunctionLibrary.git Plugins/DebugFunctionLibrary
@echo off

@echo on
git submodule add https://github.com/chainkemists/BlueprintConsoleCommands.git Plugins/BlueprintConsoleCommands
@echo off

@echo on
git submodule add https://github.com/chainkemists/MapFunctionLibrary.git Plugins/MapFunctionLibrary
@echo off

@echo on
git submodule add https://github.com/chainkemists/BlueprintUE-Cpp-Plugin.git Plugins/BlueprintUE-Cpp-Plugin
@echo off

@echo on
git submodule add https://github.com/chainkemists/Cog.git Plugins/Cog
@echo off

@echo on
git submodule add https://github.com/chainkemists/Cog.git Plugins/Cog
@echo off

@echo on
git submodule add https://github.com/chainkemists/Skelot.git Plugins/Skelot
@echo off

@echo on
git submodule add https://github.com/chainkemists/ant.git Plugins/Ant
@echo off

@echo on
git submodule add https://github.com/chainkemists/AdaptiveGizmo/ Plugins/AdaptiveGizmo
@echo off

@echo on
git submodule add https://github.com/chainkemists/SpatialQuerySystem.git Plugins/SpatialQuerySystem
@echo off

echo ===============================================================================
echo Updating CK Submodules... please wait
echo -------------------------------------------------------------------------------
echo.

@echo on
git submodule add https://github.com/chainkemists/CkApplication.git Plugins/CkApplication
@echo off

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
