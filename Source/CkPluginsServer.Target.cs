// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

[SupportedPlatforms(UnrealPlatformClass.Server)]
public class CkPluginsServerTarget : TargetRules
{
	public CkPluginsServerTarget( TargetInfo Target) : base(Target)
	{
		Type = TargetType.Server;
		bWithPushModel = true;
		DefaultBuildSettings = BuildSettingsVersion.Latest;

		ExtraModuleNames.Add("CkPlugins");
	}
}
