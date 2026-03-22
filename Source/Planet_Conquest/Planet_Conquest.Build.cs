// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Planet_Conquest : ModuleRules
{
	public Planet_Conquest(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "ProceduralMeshComponent", "UMG", "Niagara" });

		PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore", "RenderCore" });

		// Add subdirectories to include paths
		PublicIncludePaths.AddRange(new string[] {
			"Planet_Conquest/Core",
			"Planet_Conquest/World",
			"Planet_Conquest/Camera",
			"Planet_Conquest/Entities/Buildings",
			"Planet_Conquest/Entities/Cities",
			"Planet_Conquest/Entities/Resources",
			"Planet_Conquest/Entities/Vehicles",
			"Planet_Conquest/Entities/Projectiles",
			"Planet_Conquest/Entities/Kaiju",
			"Planet_Conquest/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
		
		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
