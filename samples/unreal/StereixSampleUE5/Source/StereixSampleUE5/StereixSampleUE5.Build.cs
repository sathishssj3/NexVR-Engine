using UnrealBuildTool;

public class StereixSampleUE5 : ModuleRules
{
	public StereixSampleUE5(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { 
			"Core", 
			"CoreUObject", 
			"Engine", 
			"InputCore",
			"Stereix"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });
	}
}
