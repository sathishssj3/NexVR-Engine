#pragma once

#include "CoreMinimal.h"
#include "SceneViewExtension.h"
#include "RHI.h"
#include "RenderResource.h"
#include "stereix_sdk.h"

/**
 * SceneViewExtension that hooks into Unreal's rendering pipeline.
 *
 * Game Thread:
 *   - SetupView: Calls Stereix_WaitFrame, updates head poses and projection matrices.
 *
 * Render Thread:
 *   - PostRenderViewFamily_RenderThread: Extracts D3D11 color and scene depth textures,
 *     and submits the stereoscopic frame to OpenXR via Stereix_SubmitFrameWithDepthDX11.
 */
class STEREIX_API FStereixViewExtension : public FSceneViewExtensionBase
{
public:
    FStereixViewExtension(const FAutoRegister& AutoRegister);
    virtual ~FStereixViewExtension() override;

    // FSceneViewExtensionBase interface
    virtual void SetupViewFamily(FSceneViewFamily& InViewFamily) override;
    virtual void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) override;
    virtual void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) override;
    virtual void PostRenderViewFamily_RenderThread(FRHICommandListImmediate& RHICmdList,
                                                  FSceneViewFamily& InViewFamily) override;

    /** Enable or disable depth buffer submission at runtime. */
    void SetSubmitDepthEnabled(bool bEnable) { bSubmitDepth = bEnable; }
    bool IsSubmitDepthEnabled() const { return bSubmitDepth; }

private:
    uint64_t CurrentFrameToken{0};
    bool bShouldRender{false};
    bool bSubmitDepth{true};
    float NearClipPlane{0.1f};
    float FarClipPlane{1000.0f};
};

// Backwards-compatibility alias
typedef FStereixViewExtension FNexVRViewExtension;
