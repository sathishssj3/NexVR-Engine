using UnrealBuildTool;
using System.Collections.Generic;

public class StereixSampleUE5EditorTarget : TargetRules
{
	public StereixSampleUE5EditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V4;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("StereixSampleUE5");
	}
}
