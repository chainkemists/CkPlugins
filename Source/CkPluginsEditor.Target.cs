// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using EpicGames.Core;
using System;
using System.Collections.Generic;

public class CkPluginsEditorTarget : TargetRules
{
	public CkPluginsEditorTarget( TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		bWithPushModel = true;
		DefaultBuildSettings = BuildSettingsVersion.Latest;

		// Unique build environment (a project-specific CkPluginsEditor.exe with Build/Windows/Application.ico)
		// unless this clone's Project Storage options ask for Shared, which builds engine modules into the
		// engine checkout and launches the engine's UnrealEditor.exe.
		BuildEnvironment = Get_CloneEditorEnvironment(Target.ProjectFile);

		ExtraModuleNames.Add("CkPlugins");
	}

	// Reads the untracked Saved/UnrealBuildTool/CkCloneBuildOptions.json that UnrealToolbox > Project
	// Storage writes ({"EditorEnvironment":"Shared"|"Unique"}); anything missing or unreadable is Unique.
	public static TargetBuildEnvironment Get_CloneEditorEnvironment(FileReference ProjectFile)
	{
		if (ProjectFile == null)
		{
			return TargetBuildEnvironment.Unique;
		}

		FileReference OptionsFile = FileReference.Combine(ProjectFile.Directory, "Saved", "UnrealBuildTool", "CkCloneBuildOptions.json");
		if (!FileReference.Exists(OptionsFile) || !JsonObject.TryRead(OptionsFile, out JsonObject Options))
		{
			return TargetBuildEnvironment.Unique;
		}

		return Options.TryGetStringField("EditorEnvironment", out string Environment)
			&& string.Equals(Environment, "Shared", StringComparison.OrdinalIgnoreCase)
			? TargetBuildEnvironment.Shared
			: TargetBuildEnvironment.Unique;
	}
}
