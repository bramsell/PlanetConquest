// Copyright Benjamin Ramsell. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class Planet_ConquestEditorTarget : TargetRules
{
	public Planet_ConquestEditorTarget( TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V5;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_5;
		ExtraModuleNames.Add("Planet_Conquest");
	}
}
