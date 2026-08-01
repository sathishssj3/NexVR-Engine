#include "depth_classifier.h"
#include <dxgiformat.h>

namespace vrinject {

void DepthClassifier::Classify(DepthCandidate& candidate) {
    if (!candidate.valid) return;
    
    float score = 0.0f;
    
    switch (candidate.identity.format) {
        case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
        case DXGI_FORMAT_R32G8X24_TYPELESS:
        case DXGI_FORMAT_D32_FLOAT:
        case DXGI_FORMAT_R32_TYPELESS:
            score = 1.0f; // High precision depth
            break;
            
        case DXGI_FORMAT_D24_UNORM_S8_UINT:
        case DXGI_FORMAT_R24G8_TYPELESS:
            score = 0.8f; // Standard precision depth
            break;
            
        case DXGI_FORMAT_D16_UNORM:
        case DXGI_FORMAT_R16_TYPELESS:
            score = 0.5f; // Low precision depth
            break;
            
        default:
            // Invalid or non-depth format
            candidate.valid = false;
            candidate.classifierScore = 0.0f;
            return;
    }
    
    candidate.classifierScore = score;
}

} // namespace vrinject
