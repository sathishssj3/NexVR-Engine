#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

struct NexVR_Session_T;
typedef struct NexVR_Session_T* NexVR_Session;

class FNexVRViewExtension;

/**
 * Main Unreal Engine module interface for NexVR.
 * Manages SDK initialization, session lifecycle, and SceneViewExtension registration.
 */
class NEXVR_API INexVRModule : public IModuleInterface
{
public:
    static inline INexVRModule& Get()
    {
        return FModuleManager::LoadModuleChecked<INexVRModule>("NexVR");
    }

    static inline bool IsAvailable()
    {
        return FModuleManager::Get().IsModuleLoaded("NexVR");
    }

    /** Returns the active NexVR OpenXR session handle, or nullptr if not running. */
    virtual NexVR_Session GetSession() const = 0;

    /** Whether the NexVR runtime session is actively running. */
    virtual bool IsSessionRunning() const = 0;

    /** Whether the OpenXR runtime supports hardware depth submission. */
    virtual bool SupportsDepthSubmission() const = 0;
};
