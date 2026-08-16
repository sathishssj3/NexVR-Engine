#include "rendering/neural_inpainter.h"

namespace vrinject {

NeuralInpainter::NeuralInpainter() : m_initialized(false), m_fullWidth(0), m_fullHeight(0), m_lowResWidth(0), m_lowResHeight(0) {}
NeuralInpainter::~NeuralInpainter() {}

bool NeuralInpainter::Initialize(ID3D11Device* device, const std::string& modelPath, uint32_t width, uint32_t height) {
    m_fullWidth = width;
    m_fullHeight = height;
    m_lowResWidth = width / 2;
    m_lowResHeight = height / 2;
    m_initialized = (device != nullptr);
    return m_initialized;
}

bool NeuralInpainter::CreateD3D11Resources(ID3D11Device* device) {
    if (!device) return false;
    m_initialized = true;
    return true;
}

void NeuralInpainter::Shutdown() {
    m_initialized = false;
}

// ponytail: v1.0 passthrough placeholder — no neural inference yet.
// Disocclusion is filled by the disocclusion_fill.hlsl compute shader; this
// neural path is a planned enhancement that is not wired. Upgrade path: run
// depth_inpainter.onnx (MiDaS) through ONNX Runtime with D3D11 tensor interop
// (see src/ai/midas_inpainter.h) and return the inpainted SRV instead of the
// input. Tracked as a follow-up task, not shipped in v1.0.
ID3D11ShaderResourceView* NeuralInpainter::Inpaint(ID3D11DeviceContext* context, ID3D11ShaderResourceView* warpedColorSRV, ID3D11ShaderResourceView* depthSRV) {
    if (!context || !warpedColorSRV) return nullptr;
    return warpedColorSRV; // passthrough placeholder
}


} // namespace vrinject
