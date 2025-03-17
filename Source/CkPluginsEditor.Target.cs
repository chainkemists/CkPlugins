// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class CkPluginsEditorTarget : TargetRules
{
	public CkPluginsEditorTarget( TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		bOverrideBuildEnvironment = true;
		ExtraModuleNames.Add("CkPlugins");
		DefaultBuildSettings = BuildSettingsVersion.V5;

        if (!bUseIris)
        {
            // If we enable Iris for a single target we also need to set the TargetBuildEnvironment to unique, as other projects in the solution might want it compiled out
            BuildEnvironment = TargetBuildEnvironment.Unique;
            bUseIris = true;
        }
	}
}
