#include "StereixModule.h"
#include "StereixViewExtension.h"
#include "stereix_unreal_bridge.h"
#include "stereix_sdk.h"

#include "Modules/ModuleManager.h"
#include "SceneViewExtension.h"

DEFINE_LOG_CATEGORY_STATIC(LogStereix, Log, All);

class FStereixModule : public IStereixModule
{
public:
    virtual void StartupModule() override
    {
        UE_LOG(LogStereix, Log, TEXT("Stereix Engine Unreal Module starting up."));

        // Register the SceneViewExtension to intercept stereo eye rendering and depth
        ViewExtension = FSceneViewExtensions::NewExtension<FStereixViewExtension>();
    }

    virtual void ShutdownModule() override
    {
        UE_LOG(LogStereix, Log, TEXT("Stereix Engine Unreal Module shutting down."));

        // 1. Detach the bridge first to stop the Render Thread from accessing the session
        Stereix_Unreal_Detach();

        // 2. Shut down the OpenXR session
        if (Session != nullptr)
        {
            Stereix_Shutdown(Session);
            Session = nullptr;
        }

        ViewExtension.Reset();
    }

    virtual Stereix_Session GetSession() const override
    {
        return Session;
    }

    virtual bool IsSessionRunning() const override
    {
        return Session != nullptr;
    }

    virtual bool SupportsDepthSubmission() const override
    {
        return Stereix_Unreal_SupportsDepthSubmission() != 0;
    }

private:
    Stereix_Session Session{nullptr};
    TSharedPtr<FStereixViewExtension, ESPMode::ThreadSafe> ViewExtension;
};

IMPLEMENT_MODULE(FStereixModule, Stereix)
