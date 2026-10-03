#include "StereixFunctionLibrary.h"
#include "StereixModule.h"
#include "stereix_unreal_bridge.h"
#include "stereix_sdk.h"

bool UStereixFunctionLibrary::IsSessionRunning()
{
    return IStereixModule::IsAvailable() && IStereixModule::Get().IsSessionRunning();
}

bool UStereixFunctionLibrary::SupportsDepthSubmission()
{
    return Stereix_Unreal_SupportsDepthSubmission() != 0;
}

void UStereixFunctionLibrary::GetSessionStats(int32& Submitted, int32& Dropped, int32& Failed, int32& LastResult)
{
    uint32_t Sub = 0, Drop = 0, Fail = 0;
    int32_t Res = 0;
    Stereix_Unreal_GetStats(&Sub, &Drop, &Fail, &Res);

    Submitted = static_cast<int32>(Sub);
    Dropped = static_cast<int32>(Drop);
    Failed = static_cast<int32>(Fail);
    LastResult = Res;
}

bool UStereixFunctionLibrary::SyncInput()
{
    return Stereix_Unreal_SyncInput() == 0;
}

bool UStereixFunctionLibrary::GetControllerState(int32 Hand, FStereixControllerState& OutState)
{
    Stereix_ControllerState NativeState{};
    NativeState.structSize = sizeof(Stereix_ControllerState);

    int Res = Stereix_Unreal_GetControllerState(Hand, &NativeState);
    if (Res != 0)
    {
        return false;
    }

    OutState.bIsConnected = (NativeState.isConnected != 0);
    OutState.bGripPoseValid = (NativeState.gripPoseValid != 0);
    OutState.bAimPoseValid = (NativeState.aimPoseValid != 0);

    // Convert OpenXR coordinates to Unreal Engine:
    // OpenXR: +X Right, +Y Up, -Z Forward (meters)
    // Unreal: +X Forward, +Y Right, +Z Up (centimeters)
    auto ConvertPose = [](const Stereix_Pose& P) -> FTransform
    {
        FVector Location(
            -P.position[2] * 100.0f, // Forward (-Z -> +X)
             P.position[0] * 100.0f, // Right   (+X -> +Y)
             P.position[1] * 100.0f  // Up      (+Y -> +Z)
        );

        FQuat Rotation(
            -P.orientation[2],
             P.orientation[0],
             P.orientation[1],
             P.orientation[3]
        );

        return FTransform(Rotation, Location);
    };

    OutState.GripTransform = ConvertPose(NativeState.gripPose);
    OutState.AimTransform = ConvertPose(NativeState.aimPose);

    OutState.LinearVelocity = FVector(-NativeState.linearVelocity[2] * 100.0f, NativeState.linearVelocity[0] * 100.0f, NativeState.linearVelocity[1] * 100.0f);
    OutState.AngularVelocity = FVector(FMath::RadiansToDegrees(-NativeState.angularVelocity[2]), FMath::RadiansToDegrees(NativeState.angularVelocity[0]), FMath::RadiansToDegrees(NativeState.angularVelocity[1]));

    OutState.Trigger = NativeState.trigger;
    OutState.Grip = NativeState.grip;
    OutState.Thumbstick = FVector2D(NativeState.thumbstickX, NativeState.thumbstickY);
    OutState.ButtonsDown = static_cast<int32>(NativeState.buttonsDown);
    OutState.ButtonsTouched = static_cast<int32>(NativeState.buttonsTouched);

    return true;
}

bool UStereixFunctionLibrary::TriggerHaptic(int32 Hand, float DurationMs, float FrequencyHz, float Amplitude)
{
    return Stereix_Unreal_TriggerHaptic(Hand, DurationMs, FrequencyHz, Amplitude) == 0;
}

bool UStereixFunctionLibrary::StopHaptic(int32 Hand)
{
    return Stereix_Unreal_StopHaptic(Hand) == 0;
}
