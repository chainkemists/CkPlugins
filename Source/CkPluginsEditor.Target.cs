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

		ExtraModuleNames.Add("CkPlugins");
	}
}
