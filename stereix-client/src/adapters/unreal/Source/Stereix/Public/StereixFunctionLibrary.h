#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "NexVRFunctionLibrary.generated.h"

/**
 * 6DOF controller tracking and input state for one VR motion controller.
 */
USTRUCT(BlueprintType)
struct FNexVRControllerState
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "NexVR|Input")
    bool bIsConnected{false};

    UPROPERTY(BlueprintReadOnly, Category = "NexVR|Input")
    bool bGripPoseValid{false};

    UPROPERTY(BlueprintReadOnly, Category = "NexVR|Input")
    bool bAimPoseValid{false};

    UPROPERTY(BlueprintReadOnly, Category = "NexVR|Input")
    FTransform GripTransform{FTransform::Identity};

    UPROPERTY(BlueprintReadOnly, Category = "NexVR|Input")
    FTransform AimTransform{FTransform::Identity};

    UPROPERTY(BlueprintReadOnly, Category = "NexVR|Input")
    FVector LinearVelocity{FVector::ZeroVector};

    UPROPERTY(BlueprintReadOnly, Category = "NexVR|Input")
    FVector AngularVelocity{FVector::ZeroVector};

    UPROPERTY(BlueprintReadOnly, Category = "NexVR|Input")
    float Trigger{0.0f};

    UPROPERTY(BlueprintReadOnly, Category = "NexVR|Input")
    float Grip{0.0f};

    UPROPERTY(BlueprintReadOnly, Category = "NexVR|Input")
    FVector2D Thumbstick{FVector2D::ZeroVector};

    UPROPERTY(BlueprintReadOnly, Category = "NexVR|Input")
    int32 ButtonsDown{0};

    UPROPERTY(BlueprintReadOnly, Category = "NexVR|Input")
    int32 ButtonsTouched{0};
};

/**
 * Blueprint function library for NexVR Engine integration in Unreal Engine.
 */
UCLASS()
class NEXVR_API UNexVRFunctionLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /** Check if the NexVR OpenXR session is currently running. */
    UFUNCTION(BlueprintPure, Category = "NexVR")
    static bool IsSessionRunning();

    /** Check if the OpenXR runtime supports hardware depth submission (XR_KHR_composition_layer_depth). */
    UFUNCTION(BlueprintPure, Category = "NexVR")
    static bool SupportsDepthSubmission();

    /** Retrieve frame submission diagnostics and counters from the native bridge. */
    UFUNCTION(BlueprintCallable, Category = "NexVR|Diagnostics")
    static void GetSessionStats(int32& OutSubmitted, int32& OutDropped, int32& OutFailed, int32& OutLastResult);

    // --- 6DOF Controller Input & Haptics ---

    /** Synchronize input actions from the OpenXR runtime. Call once per frame on Game Thread. */
    UFUNCTION(BlueprintCallable, Category = "NexVR|Input")
    static bool SyncInput();

    /**
     * Retrieve 6DOF tracking and input state for a motion controller.
     * @param Hand 0 = Left Hand, 1 = Right Hand.
     * @param OutState Tracking transform, velocities, analog axes, and button bitmasks.
     * @return True if state was queried successfully.
     */
    UFUNCTION(BlueprintCallable, Category = "NexVR|Input")
    static bool GetControllerState(int32 Hand, FNexVRControllerState& OutState);

    /**
     * Trigger vibration haptic feedback on a motion controller.
     * @param Hand 0 = Left Hand, 1 = Right Hand.
     * @param DurationMs Vibration duration in milliseconds (e.g. 50.0).
     * @param FrequencyHz Vibration frequency in Hz (0.0 for runtime default ~160Hz).
     * @param Amplitude Normalized vibration strength [0.0, 1.0].
     * @return True if haptic feedback was successfully scheduled.
     */
    UFUNCTION(BlueprintCallable, Category = "NexVR|Haptics")
    static bool TriggerHaptic(int32 Hand, float DurationMs = 50.0f, float FrequencyHz = 160.0f, float Amplitude = 0.8f);

    /**
     * Immediately stop any active haptic vibration on a motion controller.
     * @param Hand 0 = Left Hand, 1 = Right Hand.
     * @return True if command was executed.
     */
    UFUNCTION(BlueprintCallable, Category = "NexVR|Haptics")
    static bool StopHaptic(int32 Hand);
};
