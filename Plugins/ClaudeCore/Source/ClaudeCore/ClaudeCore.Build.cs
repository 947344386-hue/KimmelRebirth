// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ClaudeCore : ModuleRules
{
	public ClaudeCore(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"EnhancedInput",
			"UMG",
			"PhysicsCore",
			"RenderCore",
			"RHI",
			"ProceduralMeshComponent",
			"DeveloperSettings",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Slate",
			"SlateCore",
			"InputCore",
		});
	}
}
