#include "neural_inpainter.h"

namespace vrinject {

NeuralInpainter::NeuralInpainter() {}
NeuralInpainter::~NeuralInpainter() {}

bool NeuralInpainter::Initialize(ID3D11Device* device, const std::string& modelPath, uint32_t width, uint32_t height) {
    return true; // Stubbed out
}

void NeuralInpainter::Shutdown() {
}

ID3D11ShaderResourceView* NeuralInpainter::Inpaint(ID3D11DeviceContext* context, ID3D11ShaderResourceView* leftEyeView, ID3D11ShaderResourceView* rightEyeView) {
    return leftEyeView; // Stubbed out
}

} // namespace vrinject
