@echo off

echo ===============================================================================
echo Updating Marketplace Plugins Submodules... please wait
echo -------------------------------------------------------------------------------
echo.

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
git submodule add https://github.com/chainkemists/MDMetaDataEditor.git Plugins/MDMetaDataEditor
@echo off

@echo on
git submodule add https://github.com/chainkemists/NodeGraphAssistant.git Plugins/NodeGraphAssistant
@echo off

@echo on
git submodule add https://github.com/chainkemists/WorldSpaceWidgets.git Plugins/WorldSpaceWidgets
@echo off

@echo on
git submodule add https://github.com/chainkemists/ZenMode.git Plugins/ZenMode
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
git submodule add https://github.com/chainkemists/AdaptiveGizmo/ Plugins/AdaptiveGizmo
@echo off

@echo on
git submodule add https://github.com/chainkemists/SpatialQuerySystem.git Plugins/SpatialQuerySystem
@echo off

@echo on
git submodule add https://github.com/chainkemists/EOSIntegrationKit.git Plugins/EOSIntegrationKit
@echo off

@echo on
git submodule add https://github.com/chainkemists/auto-settings.git Plugins/AutoSettings
@echo offs

@echo on
git submodule add https://github.com/chainkemists/Prismatiscape-Interaction-Plugin.git Plugins/Prismatiscape-Interaction-Plugin
@echo off

@echo on
git submodule add https://github.com/chainkemists/ProInstanceTools.git Plugins/ProInstanceTools
@echo off

@echo on
git submodule add https://github.com/chainkemists/ScreenSpaceFogScattering.git Plugins/ScreenSpaceFogScattering
@echo off

@echo on
git submodule add https://github.com/chainkemists/LogViewerPro.git Plugins/LogViewerPro
@echo off

@echo on
git submodule add https://github.com/chainkemists/Projectile-Physics-Plugin.git Plugins/Projectile-Physics-Plugin
@echo off

@echo on
git submodule add https://github.com/chainkemists/UE4-EditorScriptingToolsPlugin Plugins/EditorScriptingTools
@echo off

@echo on
git submodule add https://github.com/chainkemists/PhysicalLayoutTool.git Plugins/PhysicalLayoutTool
@echo off

@echo on
git submodule add https://github.com/chainkemists/MetaCheatManager.git Plugins/MetaCheatManager
@echo off

@echo on
git submodule add https://github.com/chainkemists/CableTie.git Plugins/CableTie
@echo off

@echo on
git submodule add  https://github.com/chainkemists/AdvancedCommenting.git Plugins/AdvancedCommenting
@echo off

@echo on
git submodule add https://github.com/chainkemists/VerticalTabs.git Plugins/VerticalTabs
@echo off

echo ===============================================================================
echo Updating CK Submodules... please wait
echo -------------------------------------------------------------------------------
echo.

@echo on
git submodule add https://github.com/chainkemists/CkAuto.git CkAuto
@echo off

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
