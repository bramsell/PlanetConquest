// Copyright Benjamin Ramsell. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class Planet_ConquestTarget : TargetRules
{
	public Planet_ConquestTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V5;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_5;
		ExtraModuleNames.Add("Planet_Conquest");
	}
}
