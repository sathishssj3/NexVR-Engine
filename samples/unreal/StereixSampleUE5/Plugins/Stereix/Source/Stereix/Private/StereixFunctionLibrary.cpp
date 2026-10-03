#include "NexVRFunctionLibrary.h"
#include "NexVRModule.h"
#include "nexvr_unreal_bridge.h"

bool UNexVRFunctionLibrary::IsSessionRunning()
{
    if (INexVRModule::IsAvailable())
    {
        return INexVRModule::Get().IsSessionRunning();
    }
    return false;
}

bool UNexVRFunctionLibrary::SupportsDepthSubmission()
{
    return NexVR_Unreal_SupportsDepthSubmission() != 0;
}

void UNexVRFunctionLibrary::GetSessionStats(int32& OutSubmitted, int32& OutDropped,
                                            int32& OutFailed, int32& OutLastResult)
{
    uint32_t Submitted = 0, Dropped = 0, Failed = 0;
    int32_t LastResult = 0;

    NexVR_Unreal_GetStats(&Submitted, &Dropped, &Failed, &LastResult);

    OutSubmitted = static_cast<int32>(Submitted);
    OutDropped = static_cast<int32>(Dropped);
    OutFailed = static_cast<int32>(Failed);
    OutLastResult = LastResult;
}

bool UNexVRFunctionLibrary::SyncInput()
{
    return NexVR_Unreal_SyncInput() != 0;
}

bool UNexVRFunctionLibrary::GetControllerState(int32 Hand, FNexVRControllerState& OutState)
{
    NexVR_ControllerState NativeState{};
    NativeState.structSize = sizeof(NexVR_ControllerState);

    int Result = NexVR_Unreal_GetControllerState(Hand, &NativeState);
    if (Result == 0)
    {
        return false;
    }

    OutState.bIsConnected = (NativeState.isConnected != 0);
    OutState.bGripPoseValid = (NativeState.gripPoseValid != 0);
    OutState.bAimPoseValid = (NativeState.aimPoseValid != 0);

    // OpenXR coordinates: +X Right, +Y Up, -Z Forward in meters.
    // Unreal coordinates: +X Forward, +Y Right, +Z Up in centimeters (1m = 100cm).
    auto ConvertOpenXRPose = [](const NexVR_Pose& P) -> FTransform
    {
        // OpenXR (x, y, z) -> Unreal ( -z*100, x*100, y*100 )
        FVector Location(-P.position[2] * 100.0f, P.position[0] * 100.0f, P.position[1] * 100.0f);
        // Quaternion conversion
        FQuat Rotation(-P.orientation[2], P.orientation[0], P.orientation[1], P.orientation[3]);
        return FTransform(Rotation, Location);
    };

    OutState.GripTransform = ConvertOpenXRPose(NativeState.gripPose);
    OutState.AimTransform = ConvertOpenXRPose(NativeState.aimPose);

    OutState.LinearVelocity = FVector(-NativeState.linearVelocity[2] * 100.0f,
                                      NativeState.linearVelocity[0] * 100.0f,
                                      NativeState.linearVelocity[1] * 100.0f);
    OutState.AngularVelocity = FVector(FMath::RadiansToDegrees(-NativeState.angularVelocity[2]),
                                       FMath::RadiansToDegrees(NativeState.angularVelocity[0]),
                                       FMath::RadiansToDegrees(NativeState.angularVelocity[1]));

    OutState.Trigger = NativeState.trigger;
    OutState.Grip = NativeState.grip;
    OutState.Thumbstick = FVector2D(NativeState.thumbstickX, NativeState.thumbstickY);
    OutState.ButtonsDown = static_cast<int32>(NativeState.buttonsDown);
    OutState.ButtonsTouched = static_cast<int32>(NativeState.buttonsTouched);

    return true;
}

bool UNexVRFunctionLibrary::TriggerHaptic(int32 Hand, float DurationMs, float FrequencyHz, float Amplitude)
{
    return NexVR_Unreal_TriggerHaptic(Hand, DurationMs, FrequencyHz, Amplitude) != 0;
}

bool UNexVRFunctionLibrary::StopHaptic(int32 Hand)
{
    return NexVR_Unreal_StopHaptic(Hand) != 0;
}
