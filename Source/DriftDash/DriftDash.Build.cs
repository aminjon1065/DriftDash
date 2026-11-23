using UnrealBuildTool;

public class DriftDash : ModuleRules
{
	public DriftDash(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"ProceduralMeshComponent",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"PhysicsCore",
			"ApplicationCore",
			"RenderCore",
		});
	}
}
