#pragma once

#include <cstdint>

namespace vrinject {
namespace ue {

// Simplified FName
struct FName {
    int32_t ComparisonIndex;
    int32_t Number;
};

// Standard Unreal Engine base object
class UObject {
public:
    void** VTable;
    int32_t ObjectFlags;
    int32_t InternalIndex;
    UObject* ClassPrivate;
    FName NamePrivate;
    UObject* OuterPrivate;
};

// Global Object Array Item
struct FUObjectItem {
    UObject* Object;
    int32_t Flags;
    int32_t ClusterRootIndex;
    int32_t SerialNumber;
};

// Global Object Array
class FUObjectArray {
public:
    FUObjectItem* ObjFirst;
    FUObjectItem* ObjLast;
    int32_t MaxElements;
    int32_t NumElements;
    int32_t MaxChunks;
    int32_t NumChunks;

    // ... Chunk logic goes here ...
};

// The almighty GEngine
class UEngine : public UObject {
public:
    // This structure varies wildly between UE versions,
    // we mostly care about padding until we hit GameViewport or LocalPlayer
    // For now, this is just a placeholder.
};

// Placeholder for FSceneView which contains the projection matrices
class FSceneView {
public:
    // ...
};

} // namespace ue
} // namespace vrinject
