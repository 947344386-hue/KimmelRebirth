using UnrealBuildTool;

public class ClaudeCore : ModuleRules
{
	public ClaudeCore(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core","CoreUObject","Engine","EnhancedInput","GameplayTags",
			"GameplayTasks","UMG","NavigationSystem","AIModule","PhysicsCore",
			"ProceduralMeshComponent","RenderCore","RHI"
		});
		PrivateDependencyModuleNames.AddRange(new string[] { "Slate","SlateCore","InputCore" });
	}
}