#include "matrix_classifier.h"
#include <iostream>
#include <vector>
#include <fstream>

using namespace vrinject::ai;

// Helper to simulate reading a raw constant buffer from a game (e.g., from ID3D11Buffer)
std::vector<char> ReadMockConstantBuffer() {
    // In a real scenario, this would be:
    // D3D11_MAPPED_SUBRESOURCE mappedResource;
    // context->Map(cbuffer, 0, D3D11_MAP_READ, 0, &mappedResource);
    // ... copy mappedResource.pData ...
    
    // For this example, we'll create a 256-byte dummy buffer (4x 64-byte chunks)
    std::vector<char> buffer(256, 0);
    
    // Inject a fake Perspective Matrix at offset 64 (bytes 64-127)
    float* fPtr = reinterpret_cast<float*>(buffer.data() + 64);
    // Rough DirectX perspective matrix pattern
    fPtr[0] = 1.3f;   fPtr[1] = 0.0f;   fPtr[2] = 0.0f;  fPtr[3] = 0.0f;
    fPtr[4] = 0.0f;   fPtr[5] = 2.4f;   fPtr[6] = 0.0f;  fPtr[7] = 0.0f;
    fPtr[8] = 0.0f;   fPtr[9] = 0.0f;   fPtr[10] = 1.0f; fPtr[11] = 1.0f;
    fPtr[12] = 0.0f;  fPtr[13] = 0.0f;  fPtr[14] = -1.0f; fPtr[15] = 0.0f;

    return buffer;
}

int main() {
    std::cout << "--- VRInject AI Matrix Classifier Example ---" << std::endl;

    // 1. Initialize the classifier
    MatrixClassifier classifier;
    
    // Load the ONNX model we exported from Python
    std::wstring modelPath = L"matrix_classifier.onnx";
    if (!classifier.Initialize(modelPath)) {
        std::cerr << "Failed to initialize the matrix classifier. Is the .onnx file present?" << std::endl;
        return -1;
    }
    std::cout << "Model loaded successfully." << std::endl;

    // 2. Obtain raw constant buffer data (from your DX11 Hook)
    std::vector<char> cbufferData = ReadMockConstantBuffer();
    std::cout << "Scanning " << cbufferData.size() << " bytes of constant buffer data..." << std::endl;

    // 3. Scan the buffer for matrices
    auto detections = classifier.ScanBuffer(cbufferData.data(), cbufferData.size());

    // 4. Process the results
    if (detections.empty()) {
        std::cout << "No matrices found with high confidence." << std::endl;
    } else {
        for (const auto& result : detections) {
            int offset = result.first;
            const auto& pred = result.second;

            std::cout << "Found Matrix at byte offset [" << offset << "]" << std::endl;
            std::cout << "  -> Type: ";
            
            switch (pred.type) {
                case MatrixType::View: std::cout << "View Matrix"; break;
                case MatrixType::PerspectiveProjection: std::cout << "Perspective Projection"; break;
                case MatrixType::OrthographicProjection: std::cout << "Orthographic Projection"; break;
                case MatrixType::MVP: std::cout << "Combined MVP Matrix"; break;
                default: std::cout << "Unknown"; break;
            }
            
            std::cout << " (Confidence: " << (pred.confidence * 100.0f) << "%)" << std::endl;
        }
    }

    return 0;
}
