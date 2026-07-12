#include "stereo_pipeline.h"
#include "../core/logger.h"
#include "../core/config_manager.h"
#include "../hooks/input_hook.h"
#include <d3dcompiler.h>
#include <vector>

#pragma comment(lib, "d3dcompiler.lib")

namespace vrinject {

// Include DX11 Shaders
#include "stereo_warp_cs_dx11.h"
#include "stereo_resolve_cs_dx11.h"
#include "disocclusion_fill_cs_dx11.h"
#include "bilateral_blur_cs_dx11.h"
#include "bilateral_blend_cs_dx11.h"

// Include DX12 Shaders
#include "stereo_warp_cs_dx12.h"
#include "stereo_resolve_cs_dx12.h"
#include "disocclusion_fill_cs_dx12.h"
#include "bilateral_blur_cs_dx12.h"
#include "bilateral_blend_cs_dx12.h"

// Include Vulkan SPIR-V Shaders
const uint32_t g_stereo_warp_VK[] = 
#include "stereo_warp_cs_vk.h"
;
const uint32_t g_stereo_resolve_VK[] = 
#include "stereo_resolve_cs_vk.h"
;
const uint32_t g_disocclusion_fill_VK[] = 
#include "disocclusion_fill_cs_vk.h"
;
const uint32_t g_bilateral_blur_VK[] = 
#include "bilateral_blur_cs_vk.h"
;
const uint32_t g_bilateral_blend_VK[] = 
#include "bilateral_blend_cs_vk.h"
;

bool StereoPipeline::Initialize(IRenderer* renderer, UINT width, UINT height, const std::string& moduleDir) {
    m_renderer = renderer;
    m_width = width;
    m_height = height;



    if (!LoadShader("stereo_warp", m_warpCS)) return false;
    if (!LoadShader("stereo_resolve", m_resolveCS)) return false;
    if (!LoadShader("disocclusion_fill", m_fillCS)) return false;
    if (!LoadShader("bilateral_blur", m_blurCS)) return false;
    if (!LoadShader("bilateral_blend", m_blendCS)) return false;

    if (!CreateResources(width, height)) return false;

    // TODO: OpenXRManager needs to know the native device to initialize properly.
    // For now we will defer OpenXR init to the hook caller (e.g. DX11Hook) OR
    // we can pass native device/context into OpenXRManager via the hooks directly.
    // Since this is decoupled, we just let the hook initialize OpenXRManager and set renderer!

    // We no longer compile side-by-side PS/VS here. It will be handled 
    // by the platform hooks if they need legacy side-by-side rendering.

    // We no longer initialize ComfortGuard/NeuralInpainter in StereoPipeline directly
    // since they still depend on D3D11. They will be integrated universally in a future task.

    return true;
}

bool StereoPipeline::LoadShader(const std::string& name, ShaderHandle& outHandle) {
    if (!m_renderer) return false;

    const uint8_t* bytecode = nullptr;
    size_t size = 0;

    if (m_renderer->GetAPI() == GraphicsAPI::DX11) {
        if (name == "stereo_warp") { bytecode = g_stereo_warp_DX11; size = sizeof(g_stereo_warp_DX11); }
        else if (name == "stereo_resolve") { bytecode = g_stereo_resolve_DX11; size = sizeof(g_stereo_resolve_DX11); }
        else if (name == "disocclusion_fill") { bytecode = g_disocclusion_fill_DX11; size = sizeof(g_disocclusion_fill_DX11); }
        else if (name == "bilateral_blur") { bytecode = g_bilateral_blur_DX11; size = sizeof(g_bilateral_blur_DX11); }
        else if (name == "bilateral_blend") { bytecode = g_bilateral_blend_DX11; size = sizeof(g_bilateral_blend_DX11); }
    } else if (m_renderer->GetAPI() == GraphicsAPI::DX12) {
        if (name == "stereo_warp") { bytecode = g_stereo_warp_DX12; size = sizeof(g_stereo_warp_DX12); }
        else if (name == "stereo_resolve") { bytecode = g_stereo_resolve_DX12; size = sizeof(g_stereo_resolve_DX12); }
        else if (name == "disocclusion_fill") { bytecode = g_disocclusion_fill_DX12; size = sizeof(g_disocclusion_fill_DX12); }
        else if (name == "bilateral_blur") { bytecode = g_bilateral_blur_DX12; size = sizeof(g_bilateral_blur_DX12); }
        else if (name == "bilateral_blend") { bytecode = g_bilateral_blend_DX12; size = sizeof(g_bilateral_blend_DX12); }
    } else if (m_renderer->GetAPI() == GraphicsAPI::VULKAN) {
        if (name == "stereo_warp") { bytecode = (const uint8_t*)g_stereo_warp_VK; size = sizeof(g_stereo_warp_VK); }
        else if (name == "stereo_resolve") { bytecode = (const uint8_t*)g_stereo_resolve_VK; size = sizeof(g_stereo_resolve_VK); }
        else if (name == "disocclusion_fill") { bytecode = (const uint8_t*)g_disocclusion_fill_VK; size = sizeof(g_disocclusion_fill_VK); }
        else if (name == "bilateral_blur") { bytecode = (const uint8_t*)g_bilateral_blur_VK; size = sizeof(g_bilateral_blur_VK); }
        else if (name == "bilateral_blend") { bytecode = (const uint8_t*)g_bilateral_blend_VK; size = sizeof(g_bilateral_blend_VK); }
    }

    if (bytecode == nullptr || size == 0) {
        LOG_ERROR("Shader %s not found for current API", name.c_str());
        return false;
    }

    outHandle = m_renderer->LoadComputeShader(bytecode, size);
    return (outHandle.shaderBytecode != nullptr || outHandle.pipelineState != nullptr);
}
// LoadShader handles compilation to IRenderer

bool StereoPipeline::CreateResources(UINT width, UINT height) {
    if (!m_renderer) return false;

    // We assume the game format is an 8-bit or 10-bit UNORM (e.g. DXGI_FORMAT_R8G8B8A8_UNORM / VK_FORMAT_R8G8B8A8_UNORM)
    // IRenderer maps this parameter directly to the native format integer.
    // For universal injection, we pass 28 which maps to DXGI_FORMAT_R8G8B8A8_UNORM 
    // and VK_FORMAT_R8G8B8A8_UNORM (actually Vulkan's is 37, but the renderer wrappers can map it).
    // For now we'll pass 28.
    uint32_t colorFmt = 28; // DXGI_FORMAT_R8G8B8A8_UNORM
    if (m_renderer->GetAPI() == GraphicsAPI::VULKAN) {
        colorFmt = 37; // VK_FORMAT_R8G8B8A8_UNORM
    }

    m_rightEyeTex = m_renderer->CreateTexture(width, height, colorFmt, true);
    if (!m_rightEyeTex.nativePtr) return false;

    m_blurTempTex = m_renderer->CreateTexture(width, height, colorFmt, true);
    if (!m_blurTempTex.nativePtr) return false;

    m_leftEyeTex = m_renderer->CreateTexture(width, height, colorFmt, true);
    if (!m_leftEyeTex.nativePtr) return false;

    uint32_t depthFmt = 43; // DXGI_FORMAT_R32_UINT
    if (m_renderer->GetAPI() == GraphicsAPI::VULKAN) {
        depthFmt = 98; // VK_FORMAT_R32_UINT
    }

    m_warpBufferTex = m_renderer->CreateTexture(width, height, depthFmt, true);
    if (!m_warpBufferTex.nativePtr) return false;

    // Profiling queries and samplers are omitted in the universal framework MVP
    // They will be re-added via IRenderer extension later if needed.

    return true;
}

void StereoPipeline::RenderComputeOnly(TextureHandle colorSRV, TextureHandle depthSRV, const StereoParams& params) {
    if (!m_renderer) return;

    m_leftEyeTex = colorSRV;

    uint32_t clearValue[4] = {0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF};
    LOG_INFO("RenderComputeOnly: ClearUAVUint");
    m_renderer->ClearUAVUint(m_warpBufferTex, clearValue);

    LOG_INFO("RenderComputeOnly: Warp Pass");
    TextureHandle warpInputs[] = { colorSRV, depthSRV };
    TextureHandle warpOutputs[] = { m_warpBufferTex };
    m_renderer->DispatchCompute(m_warpCS, warpInputs, 2, warpOutputs, 1, &params, sizeof(StereoParams), (m_width + 7) / 8, (m_height + 7) / 8);

    LOG_INFO("RenderComputeOnly: Resolve Pass");
    TextureHandle resolveInputs[] = { colorSRV, m_warpBufferTex };
    TextureHandle resolveOutputs[] = { m_rightEyeTex };
    m_renderer->DispatchCompute(m_resolveCS, resolveInputs, 2, resolveOutputs, 1, &params, sizeof(StereoParams), (m_width + 7) / 8, (m_height + 7) / 8);

    LOG_INFO("RenderComputeOnly: Fill Pass");
    TextureHandle fillOutputs[] = { m_rightEyeTex };
    m_renderer->DispatchCompute(m_fillCS, nullptr, 0, fillOutputs, 1, &params, sizeof(StereoParams), (m_width + 15) / 16, (m_height + 15) / 16);

    LOG_INFO("RenderComputeOnly: Blur Pass");
    TextureHandle blurInputs[] = { m_rightEyeTex };
    TextureHandle blurOutputs[] = { m_blurTempTex };
    m_renderer->DispatchCompute(m_blurCS, blurInputs, 1, blurOutputs, 1, &params, sizeof(StereoParams), (m_width + 15) / 16, (m_height + 15) / 16);

    LOG_INFO("RenderComputeOnly: CopyTexture");
    m_renderer->CopyTexture(m_rightEyeTex, m_blurTempTex);
    LOG_INFO("RenderComputeOnly: Done");
}
// Legacy logic removed

void StereoPipeline::Shutdown() {
    if (!m_renderer) return;

    if (m_warpCS.shaderBytecode || m_warpCS.pipelineState) m_renderer->DestroyShader(m_warpCS);
    if (m_resolveCS.shaderBytecode || m_resolveCS.pipelineState) m_renderer->DestroyShader(m_resolveCS);
    if (m_fillCS.shaderBytecode || m_fillCS.pipelineState) m_renderer->DestroyShader(m_fillCS);
    if (m_blurCS.shaderBytecode || m_blurCS.pipelineState) m_renderer->DestroyShader(m_blurCS);
    if (m_blendCS.shaderBytecode || m_blendCS.pipelineState) m_renderer->DestroyShader(m_blendCS);

    if (m_rightEyeTex.nativePtr) m_renderer->DestroyTexture(m_rightEyeTex);
    if (m_blurTempTex.nativePtr) m_renderer->DestroyTexture(m_blurTempTex);
    if (m_warpBufferTex.nativePtr) m_renderer->DestroyTexture(m_warpBufferTex);
    // m_leftEyeTex is an external texture (swapchain) assigned dynamically, do not destroy it here.

    m_openxrManager.Shutdown();
}

} // namespace vrinject
