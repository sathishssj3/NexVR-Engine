#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "StereixFunctionLibrary.generated.h"

/**
 * 6DOF controller tracking and input state for one VR motion controller.
 */
USTRUCT(BlueprintType)
struct FStereixControllerState
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Stereix|Input")
    bool bIsConnected{false};

    UPROPERTY(BlueprintReadOnly, Category = "Stereix|Input")
    bool bGripPoseValid{false};

    UPROPERTY(BlueprintReadOnly, Category = "Stereix|Input")
    bool bAimPoseValid{false};

    UPROPERTY(BlueprintReadOnly, Category = "Stereix|Input")
    FTransform GripTransform{FTransform::Identity};

    UPROPERTY(BlueprintReadOnly, Category = "Stereix|Input")
    FTransform AimTransform{FTransform::Identity};

    UPROPERTY(BlueprintReadOnly, Category = "Stereix|Input")
    FVector LinearVelocity{FVector::ZeroVector};

    UPROPERTY(BlueprintReadOnly, Category = "Stereix|Input")
    FVector AngularVelocity{FVector::ZeroVector};

    UPROPERTY(BlueprintReadOnly, Category = "Stereix|Input")
    float Trigger{0.0f};

    UPROPERTY(BlueprintReadOnly, Category = "Stereix|Input")
    float Grip{0.0f};

    UPROPERTY(BlueprintReadOnly, Category = "Stereix|Input")
    FVector2D Thumbstick{FVector2D::ZeroVector};

    UPROPERTY(BlueprintReadOnly, Category = "Stereix|Input")
    int32 ButtonsDown{0};

    UPROPERTY(BlueprintReadOnly, Category = "Stereix|Input")
    int32 ButtonsTouched{0};
};

// Backwards-compatibility alias
typedef FStereixControllerState FNexVRControllerState;

/**
 * Blueprint function library for Stereix Engine integration in Unreal Engine.
 */
UCLASS()
class STEREIX_API UStereixFunctionLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /** Checks whether the Stereix VR runtime session is actively running. */
    UFUNCTION(BlueprintPure, Category = "Stereix|Session")
    static bool IsSessionRunning();

    /** Checks whether the active OpenXR runtime supports hardware depth buffer submission. */
    UFUNCTION(BlueprintPure, Category = "Stereix|Session")
    static bool SupportsDepthSubmission();

    /** Retrieves live frame submission performance statistics. */
    UFUNCTION(BlueprintCallable, Category = "Stereix|Diagnostics")
    static void GetSessionStats(int32& Submitted, int32& Dropped, int32& Failed, int32& LastResult);

    /** Synchronizes controller inputs from OpenXR. Call once per frame on Game Thread before polling controllers. */
    UFUNCTION(BlueprintCallable, Category = "Stereix|Input")
    static bool SyncInput();

    /** Reads 6DOF controller tracking pose and inputs (Hand: 0 = Left, 1 = Right). */
    UFUNCTION(BlueprintCallable, Category = "Stereix|Input")
    static bool GetControllerState(int32 Hand, FStereixControllerState& OutState);

    /** Triggers haptic vibration pulse on a controller. */
    UFUNCTION(BlueprintCallable, Category = "Stereix|Input")
    static bool TriggerHaptic(int32 Hand, float DurationMs = 50.0f, float FrequencyHz = 0.0f, float Amplitude = 0.5f);

    /** Stops active haptic vibration on a controller. */
    UFUNCTION(BlueprintCallable, Category = "Stereix|Input")
    static bool StopHaptic(int32 Hand);
};

// Backwards-compatibility alias
typedef UStereixFunctionLibrary UNexVRFunctionLibrary;
