#pragma once
#include <cstdint>
#include <tuple>
#include "core/graphics_types.h"

namespace vrinject {

using DepthResourceIdentity = GraphicsResourceIdentity;

// Hash operator for std::unordered_map (needs to be specialized in std namespace)
} // namespace vrinject

namespace std {
    template <>
    struct hash<vrinject::GraphicsResourceIdentity> {
        std::size_t operator()(const vrinject::GraphicsResourceIdentity& id) const {
            return std::hash<void*>()(id.nativeHandle) ^ 
                   (std::hash<uint64_t>()(id.epoch.resourceGeneration) << 1);
        }
    };
}
