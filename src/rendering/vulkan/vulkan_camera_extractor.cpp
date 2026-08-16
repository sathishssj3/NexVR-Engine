#include "rendering/vulkan/vulkan_camera_extractor.h"
#include "rendering/vulkan/vulkan_buffer_resolver.h"
#include "rendering/vulkan/vulkan_descriptor_tracker.h"
#include "heuristics/candidate_collector.h"
#include "heuristics/camera_classifier.h"
#include "core/math_types.h"

namespace vrinject {
namespace vulkan {

void VulkanCameraExtractor::OnBindDescriptorSets(
    VkCommandBuffer commandBuffer,
    VkPipelineBindPoint pipelineBindPoint,
    VkPipelineLayout layout,
    uint32_t firstSet,
    uint32_t descriptorSetCount,
    const VkDescriptorSet* pDescriptorSets,
    uint32_t dynamicOffsetCount,
    const uint32_t* pDynamicOffsets)
{
    if (!commandBuffer || pipelineBindPoint != VK_PIPELINE_BIND_POINT_GRAPHICS) return;

    std::unique_lock<std::shared_mutex> lock(m_mutex);
    auto& state = m_commandBufferStates[commandBuffer];
    state.pipelineLayout = layout;

    uint32_t offsetIndex = 0;
    for (uint32_t i = 0; i < descriptorSetCount; ++i) {
        VkDescriptorSet set = pDescriptorSets[i];
        if (!set) continue;

        BoundDescriptorSet boundSet;
        boundSet.set = set;

        // Note: we don't know exactly which bindings have dynamic offsets from just this call, 
        // Vulkan passes them in order of dynamic bindings. 
        // For accurate tracking, we'd need pipeline layout reflection. 
        // We will just store all of them in the set structure for simplistic matching,
        // or just apply them linearly. To be safe, we'll copy all provided offsets to the first set 
        // or spread them (this is a known simplification for the PoC).
        // Realistically, the dynamic offsets array corresponds to bindings across ALL sets being bound.
        while (offsetIndex < dynamicOffsetCount) {
            boundSet.dynamicOffsets.push_back(pDynamicOffsets[offsetIndex++]);
        }

        state.boundSets[firstSet + i] = std::move(boundSet);
    }
}

void VulkanCameraExtractor::OnExecuteCommands(
    VkCommandBuffer commandBuffer,
    uint32_t commandBufferCount,
    const VkCommandBuffer* pCommandBuffers)
{
    if (!commandBuffer || commandBufferCount == 0 || !pCommandBuffers) return;

    std::unique_lock<std::shared_mutex> lock(m_mutex);
    auto& state = m_commandBufferStates[commandBuffer];
    for (uint32_t i = 0; i < commandBufferCount; ++i) {
        state.executedSecondaries.push_back(pCommandBuffers[i]);
    }
}

void VulkanCameraExtractor::Extract(VkDevice device, uint32_t submitCount, const VkSubmitInfo* pSubmits) {
    if (!device || !pSubmits || submitCount == 0) return;

    std::shared_lock<std::shared_mutex> lock(m_mutex);
    auto& resolver = VulkanBufferResolver::Get();
    auto& descriptorTracker = VulkanDescriptorTracker::Get();

    for (uint32_t s = 0; s < submitCount; ++s) {
        const auto& submit = pSubmits[s];
        
        // We'll process both primary command buffers and any secondary command buffers they execute
        std::vector<VkCommandBuffer> buffersToProcess;
        for (uint32_t c = 0; c < submit.commandBufferCount; ++c) {
            buffersToProcess.push_back(submit.pCommandBuffers[c]);
        }

        // Gather all secondary command buffers dynamically
        for (size_t i = 0; i < buffersToProcess.size(); ++i) {
            VkCommandBuffer cb = buffersToProcess[i];
            auto it = m_commandBufferStates.find(cb);
            if (it != m_commandBufferStates.end()) {
                for (VkCommandBuffer secondary : it->second.executedSecondaries) {
                    buffersToProcess.push_back(secondary);
                }
            }
        }

        for (VkCommandBuffer cb : buffersToProcess) {
            auto it = m_commandBufferStates.find(cb);
            if (it == m_commandBufferStates.end()) continue;

            const auto& state = it->second;

            // Iterate over all bound descriptor sets
            for (const auto& pair : state.boundSets) {
                const auto& boundSet = pair.second;
                
                VulkanDescriptorSetInfo setInfo;
                if (!descriptorTracker.GetDescriptorSetInfo(device, boundSet.set, setInfo)) continue;

                // Known Limitation: Push constants are occasionally used for projections.
                // Tracking vkCmdPushConstants is needed to support those engines. Currently we only scan descriptor-bound uniform buffers.

                // Scan all bindings in the descriptor set
                for (const auto& bindingPair : setInfo.bindings) {
                    uint32_t binding = bindingPair.first;
                    
                    // Simplified dynamic offset retrieval (usually we need to know if this binding is dynamic)
                    uint32_t dynamicOffset = 0;
                    if (!boundSet.dynamicOffsets.empty()) {
                        dynamicOffset = boundSet.dynamicOffsets[0]; // Simplified
                    }

                    ResolvedBufferInfo bufInfo = resolver.ResolveBuffer(device, boundSet.set, binding, dynamicOffset);
                    if (bufInfo.IsValid() && bufInfo.size >= sizeof(Matrix4x4)) {
                        // Scan buffer for matrices
                        const size_t numMatrices = bufInfo.size / sizeof(Matrix4x4);
                        const Matrix4x4* matrices = reinterpret_cast<const Matrix4x4*>(bufInfo.mappedPointer);

                        for (size_t m = 0; m < numMatrices; ++m) {
                            uint64_t id = reinterpret_cast<uint64_t>(bufInfo.mappedPointer) + (m * sizeof(Matrix4x4));

                            CameraCandidate candidate;
                            if (CameraClassifier::TryCreateCandidate(id, bufInfo.mappedPointer, matrices[m], candidate)) {
                                CandidateCollector::Get().SubmitCandidate(candidate);
                            }
                        }
                    }
                }
            }
        }
    }
}

} // namespace vulkan
} // namespace vrinject
