#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "stereix_sdk.h"

class FStereixViewExtension;

/**
 * Main Unreal Engine module interface for Stereix Engine.
 * Manages SDK initialization, session lifecycle, and SceneViewExtension registration.
 */
class STEREIX_API IStereixModule : public IModuleInterface
{
public:
    static inline IStereixModule& Get()
    {
        return FModuleManager::LoadModuleChecked<IStereixModule>("Stereix");
    }

    static inline bool IsAvailable()
    {
        return FModuleManager::Get().IsModuleLoaded("Stereix");
    }

    /** Returns the active Stereix OpenXR session handle, or nullptr if not running. */
    virtual Stereix_Session GetSession() const = 0;

    /** Whether the Stereix runtime session is actively running. */
    virtual bool IsSessionRunning() const = 0;

    /** Whether the OpenXR runtime supports hardware depth submission. */
    virtual bool SupportsDepthSubmission() const = 0;
};

// Backwards-compatibility alias
typedef IStereixModule INexVRModule;
