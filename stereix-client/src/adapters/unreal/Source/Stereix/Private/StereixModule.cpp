#include "NexVRModule.h"
#include "NexVRViewExtension.h"
#include "nexvr_unreal_bridge.h"
#include "nexvr_sdk.h"

#include "Modules/ModuleManager.h"
#include "SceneViewExtension.h"

DEFINE_LOG_CATEGORY_STATIC(LogNexVR, Log, All);

class FNexVRModule : public INexVRModule
{
public:
    virtual void StartupModule() override
    {
        UE_LOG(LogNexVR, Log, TEXT("NexVR Engine Unreal Module starting up."));

        // Register the SceneViewExtension to intercept stereo eye rendering and depth
        ViewExtension = FSceneViewExtensions::NewExtension<FNexVRViewExtension>();
    }

    virtual void ShutdownModule() override
    {
        UE_LOG(LogNexVR, Log, TEXT("NexVR Engine Unreal Module shutting down."));

        // 1. Detach the bridge first to stop the Render Thread from accessing the session
        NexVR_Unreal_Detach();

        // 2. Shut down the OpenXR session
        if (Session != nullptr)
        {
            NexVR_Shutdown(Session);
            Session = nullptr;
        }

        ViewExtension.Reset();
    }

    virtual NexVR_Session GetSession() const override
    {
        return Session;
    }

    virtual bool IsSessionRunning() const override
    {
        return Session != nullptr;
    }

    virtual bool SupportsDepthSubmission() const override
    {
        return NexVR_Unreal_SupportsDepthSubmission() != 0;
    }

private:
    NexVR_Session Session{nullptr};
    TSharedPtr<FNexVRViewExtension, ESPMode::ThreadSafe> ViewExtension;
};

IMPLEMENT_MODULE(FNexVRModule, NexVR)
