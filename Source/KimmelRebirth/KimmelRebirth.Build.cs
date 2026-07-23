// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class KimmelRebirth : ModuleRules
{
	public KimmelRebirth(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"KimmelRebirth",
			"KimmelRebirth/Variant_Platforming",
			"KimmelRebirth/Variant_Platforming/Animation",
			"KimmelRebirth/Variant_Combat",
			"KimmelRebirth/Variant_Combat/AI",
			"KimmelRebirth/Variant_Combat/Animation",
			"KimmelRebirth/Variant_Combat/Gameplay",
			"KimmelRebirth/Variant_Combat/Interfaces",
			"KimmelRebirth/Variant_Combat/UI",
			"KimmelRebirth/Variant_SideScrolling",
			"KimmelRebirth/Variant_SideScrolling/AI",
			"KimmelRebirth/Variant_SideScrolling/Gameplay",
			"KimmelRebirth/Variant_SideScrolling/Interfaces",
			"KimmelRebirth/Variant_SideScrolling/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
