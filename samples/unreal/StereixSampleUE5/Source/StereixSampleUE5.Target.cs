using UnrealBuildTool;
using System.Collections.Generic;

public class StereixSampleUE5Target : TargetRules
{
	public StereixSampleUE5Target(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V4;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("StereixSampleUE5");
	}
}
