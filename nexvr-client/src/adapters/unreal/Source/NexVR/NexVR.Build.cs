// NexVR Engine Unreal Build Tool (UBT) Module Definition.
//
// Compatible with Unreal Engine 4.27 through Unreal Engine 5.5.
// Integrates NexVR SDK and Native RHI Bridge into Unreal Engine's render pipeline.

using System;
using System.IO;
using UnrealBuildTool;

public class NexVR : ModuleRules
{
    public NexVR(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicIncludePaths.AddRange(
            new string[] {
                Path.Combine(ModuleDirectory, "Public")
            }
        );

        PrivateIncludePaths.AddRange(
            new string[] {
                Path.Combine(ModuleDirectory, "Private"),
                Path.Combine(PluginDirectory, "Source", "ThirdParty", "NexVR", "include")
            }
        );

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "CoreUObject",
                "Engine",
                "InputCore",
                "RHI",
                "RenderCore",
                "Renderer"
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "D3D11RHI",
                "D3D12RHI"
            }
        );

        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            // Link nexvr_sdk.lib import library
            string SdkLibPath = Path.Combine(PluginDirectory, "Binaries", "Win64");
            PublicAdditionalLibraries.Add(Path.Combine(SdkLibPath, "nexvr_sdk.lib"));
            PublicAdditionalLibraries.Add(Path.Combine(SdkLibPath, "nexvr_unreal.lib"));

            // Delay-load runtime DLLs so initialization order is fully controlled
            PublicDelayLoadDLLs.Add("nexvr_sdk.dll");
            PublicDelayLoadDLLs.Add("nexvr_unreal.dll");

            // Stage DLLs beside the game executable in packaged builds
            RuntimeDependencies.Add(Path.Combine(PluginDirectory, "Binaries", "Win64", "nexvr_sdk.dll"));
            RuntimeDependencies.Add(Path.Combine(PluginDirectory, "Binaries", "Win64", "nexvr_unreal.dll"));
        }
    }
}
