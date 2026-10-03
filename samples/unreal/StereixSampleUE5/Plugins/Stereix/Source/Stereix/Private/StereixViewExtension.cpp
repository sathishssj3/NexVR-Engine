#include "StereixViewExtension.h"
#include "stereix_unreal_bridge.h"
#include "stereix_sdk.h"

#include "RHI.h"
#include "RHICommandList.h"
#include "SceneView.h"
#include "SceneRendering.h"
#include "Engine/Engine.h"

FStereixViewExtension::FStereixViewExtension(const FAutoRegister& AutoRegister)
    : FSceneViewExtensionBase(AutoRegister)
{
    NearClipPlane = GNearClippingPlane / 100.0f; // Unreal centimeters to meters
    if (NearClipPlane <= 0.0f)
    {
        NearClipPlane = 0.05f;
    }
}

FStereixViewExtension::~FStereixViewExtension()
{
}

void FStereixViewExtension::SetupViewFamily(FSceneViewFamily& InViewFamily)
{
    // Ensure viewport family is configured for stereo if active
}

void FStereixViewExtension::SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView)
{
    // Game Thread: Wait for next frame pose from runtime throttle
    // In production, this locates the view pose and pairs the frame token
}

void FStereixViewExtension::BeginRenderViewFamily(FSceneViewFamily& InViewFamily)
{
}

void FStereixViewExtension::PostRenderViewFamily_RenderThread(FRHICommandListImmediate& RHICmdList,
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
    if (bSubmitDepth && Stereix_Unreal_SupportsDepthSubmission())
    {
        // Unreal Engine uses Reversed-Z depth projection (Near=1.0, Far=0.0)
        // Stage with reversed depth range
        int Slot = Stereix_Unreal_StageFrameWithDepth(
            CurrentFrameToken,
            NativeColor,
            NativeDepth,
            NearClipPlane,
            FarClipPlane,
            STEREIX_DEPTH_RANGE_REVERSED
        );

        if (Slot >= 0)
        {
            Stereix_Unreal_ProcessRenderCommand(Slot);
        }
    }
    else
    {
        int Slot = Stereix_Unreal_StageFrame(CurrentFrameToken, NativeColor);
        if (Slot >= 0)
        {
            Stereix_Unreal_ProcessRenderCommand(Slot);
        }
    }
}
