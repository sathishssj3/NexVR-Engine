#include "dx11_graphics_backend.h"
#include "../core/camera_lock_manager.h"
#include "../rendering/dx11_graphics_backend.h"
#include "../core/camera_lock_manager.h"
#include "../core/depth_lock_manager.h"
#include "../rendering/stereo_pipeline.h"

namespace vrinject {

DX11GraphicsBackend::DX11GraphicsBackend() {
}

DX11GraphicsBackend::~DX11GraphicsBackend() {
    Shutdown();
}

bool DX11GraphicsBackend::Initialize(void* nativeDevice, void* nativeContext) {
    m_device = static_cast<ID3D11Device*>(nativeDevice);
    m_context = static_cast<ID3D11DeviceContext*>(nativeContext);
    m_resourceManager = std::make_unique<StereoResourceManager>(m_device);
    m_stereoRenderer = std::make_unique<StereoRenderer>();
    return m_stereoRenderer != nullptr;
}

void DX11GraphicsBackend::Shutdown() {
    m_stereoRenderer.reset();
    m_resourceManager.reset();
    m_device = nullptr;
    m_context = nullptr;
}

void DX11GraphicsBackend::SetState(StereoRendererState state) {
    if (m_stereoRenderer) m_stereoRenderer->SetState(state);
}

StereoRendererState DX11GraphicsBackend::GetState() const {
    return m_stereoRenderer ? m_stereoRenderer->GetState() : StereoRendererState::UNINITIALIZED;
}

bool DX11GraphicsBackend::Healthy() const {
    return m_device != nullptr && m_context != nullptr;
}

CameraSnapshot DX11GraphicsBackend::GetCamera() {
    return CameraLockManager::Get().GetSnapshot();
}

DepthSnapshot DX11GraphicsBackend::GetDepth() {
    return DepthLockManager::Get().GetSnapshot();
}

void* DX11GraphicsBackend::GetLeftEyeTexture() {
    return m_resourceManager ? m_resourceManager->GetLeftEyeTexture() : nullptr;
}

void* DX11GraphicsBackend::GetRightEyeTexture() {
    return m_resourceManager ? m_resourceManager->GetRightEyeTexture() : nullptr;
}

void DX11GraphicsBackend::RenderStereo(
    const CameraSnapshot& camSnapshot, 
    const DepthSnapshot& depthSnapshot, 
    const StereoParams& params
) {
    if (!m_stereoRenderer || !m_context) return;
    
    // Ensure resource manager is initialized to the current resolution
    DXGI_FORMAT targetFormat = DXGI_FORMAT_R8G8B8A8_UNORM; // Should come from snapshot, defaulting for now
    uint32_t width = depthSnapshot.identity.width;
    uint32_t height = depthSnapshot.identity.height;
    if (width == 0) width = 1024;
    if (height == 0) height = 1024;
    
    if (!m_resourceManager->Initialize(width, height, targetFormat)) {
        return;
    }
    
    StereoConstants constants;
    constants.ipd = params.ipd;
    constants.convergence = params.convergence;
    
    EyeView leftEye, rightEye;
    StereoCameraGenerator::Generate(camSnapshot, constants, leftEye, rightEye);
    
    StereoFrameContext frameCtx = StereoFrameBuilder::Build(
        camSnapshot.frame,
        camSnapshot, 
        depthSnapshot,
        leftEye,
        rightEye,
        constants,
        width,
        height
    );
    
    // The compute shader samples the game's colour and depth. Those live behind
    // the resource identities the lock managers captured this frame: colour is
    // the swapchain backbuffer stamped in by FrameCoordinator, depth is the
    // locked depth target. Both arrive as raw ID3D11Texture2D*.
    auto* gameColorTex = static_cast<ID3D11Texture2D*>(camSnapshot.resourceIdentity.nativeHandle);
    auto* gameDepthTex = static_cast<ID3D11Texture2D*>(depthSnapshot.identity.nativeHandle);

    // Bail rather than dispatch with nulls. RenderStereoFrame would latch
    // StereoRendererState::FAILED, which is sticky and would disable stereo for
    // the rest of the session over what is usually a single bad frame.
    if (!m_resourceManager->EnsureSourceViews(m_context, gameColorTex, gameDepthTex)) {
        return;
    }

    m_stereoRenderer->RenderStereoFrame(
        m_context,
        m_resourceManager.get(),
        frameCtx,
        m_resourceManager->GetGameColorSRV(),
        m_resourceManager->GetGameDepthSRV()
    );
}

} // namespace vrinject
