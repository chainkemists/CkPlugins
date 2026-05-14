// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class CkPluginsEditorTarget : TargetRules
{
	public CkPluginsEditorTarget( TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		bWithPushModel = true;
		DefaultBuildSettings = BuildSettingsVersion.Latest;

		// Unique build environment produces a project-specific CkPluginsEditor.exe
		// in $(ProjectDir)/Binaries/Win64/ with Build/Windows/Application.ico embedded
		// by rc.exe. Without this, Launch defaults to the engine's stock UnrealEditor.exe
		// and the project icon never reaches the Windows taskbar.
		BuildEnvironment = TargetBuildEnvironment.Unique;

		ExtraModuleNames.Add("CkPlugins");
	}
}
