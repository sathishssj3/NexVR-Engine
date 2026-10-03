#include "NexVRViewExtension.h"
#include "nexvr_unreal_bridge.h"
#include "nexvr_sdk.h"

#include "RHI.h"
#include "RHICommandList.h"
#include "SceneView.h"
#include "SceneRendering.h"
#include "Engine/Engine.h"

FNexVRViewExtension::FNexVRViewExtension(const FAutoRegister& AutoRegister)
    : FSceneViewExtensionBase(AutoRegister)
{
    NearClipPlane = GNearClippingPlane / 100.0f; // Unreal centimeters to meters
    if (NearClipPlane <= 0.0f)
    {
        NearClipPlane = 0.05f;
    }
}

FNexVRViewExtension::~FNexVRViewExtension()
{
}

void FNexVRViewExtension::SetupViewFamily(FSceneViewFamily& InViewFamily)
{
    // Ensure viewport family is configured for stereo if active
}

void FNexVRViewExtension::SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView)
{
    // Game Thread: Wait for next frame pose from runtime throttle
    // In production, this locates the view pose and pairs the frame token
}

void FNexVRViewExtension::BeginRenderViewFamily(FSceneViewFamily& InViewFamily)
{
}

void FNexVRViewExtension::PostRenderViewFamily_RenderThread(FRHICommandListImmediate& RHICmdList,
                                                           FSceneViewFamily& InViewFamily)
{
    check(IsInRenderingThread());

    if (InViewFamily.RenderTarget == nullptr)
    {
        return;
    }

    FRHITexture* ColorTextureRHI = InViewFamily.RenderTarget->GetRenderTargetTexture();
    if (ColorTextureRHI == nullptr)
    {
        return;
    }

    void* NativeColor = ColorTextureRHI->GetNativeResource();
    void* NativeDepth = nullptr;

    // Retrieve depth texture if supported and enabled
    if (bSubmitDepth && NexVR_Unreal_SupportsDepthSubmission())
    {
        // On D3D11 RHI, SceneDepthTexture provides the underlying ID3D11Texture2D*
        // NativeDepth = SceneDepthTexture->GetNativeResource();
    }

    // Unreal Engine uses Reverse-Z (near=1.0, far=0.0) by default across all platforms.
    const int DepthRange = 1; // NEXVR_DEPTH_RANGE_REVERSED

    const FString RHIName = GDynamicRHI ? GDynamicRHI->GetName() : TEXT("");
    const bool bIsD3D12 = RHIName.Contains(TEXT("D3D12"));

    int Slot = -1;
    if (bIsD3D12)
    {
        // D3D12: NativeColor is ID3D12Resource*
        Slot = NexVR_Unreal_StageFrameWithDepthDX12(CurrentFrameToken, NativeColor, 0, NativeDepth, 0,
                                                    NearClipPlane, FarClipPlane, DepthRange);
    }
    else
    {
        // D3D11: NativeColor is ID3D11Texture2D*
        Slot = NexVR_Unreal_StageFrameWithDepth(CurrentFrameToken, NativeColor, NativeDepth,
                                                NearClipPlane, FarClipPlane, DepthRange);
    }

    if (Slot >= 0)
    {
        NexVR_Unreal_ProcessRenderCommand(Slot);
    }
}
