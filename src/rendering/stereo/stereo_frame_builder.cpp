#include "rendering/stereo/stereo_frame_builder.h"
#include <DirectXMath.h>

namespace vrinject {

static Matrix4x4 InverseMatrix(const Matrix4x4& m) {
    DirectX::XMMATRIX dxm = DirectX::XMLoadFloat4x4(reinterpret_cast<const DirectX::XMFLOAT4X4*>(&m));
    DirectX::XMVECTOR det;
    DirectX::XMMATRIX invDxm = DirectX::XMMatrixInverse(&det, dxm);
    
    Matrix4x4 result;
    DirectX::XMStoreFloat4x4(reinterpret_cast<DirectX::XMFLOAT4X4*>(&result), invDxm);
    return result;
}

StereoFrameContext StereoFrameBuilder::Build(uint64_t frameId,
                                             const CameraSnapshot& camera,
                                             const DepthSnapshot& depth,
                                             const EyeView& leftEye,
                                             const EyeView& rightEye,
                                             const StereoConstants& constants,
                                             uint32_t width,
                                             uint32_t height) {
    StereoFrameContext ctx;
    ctx.frameId = frameId;
    ctx.camera = camera;
    ctx.depth = depth;
    ctx.leftEye = leftEye;
    ctx.rightEye = rightEye;
    ctx.constants = constants;
    
    ctx.viewport.width = width;
    ctx.viewport.height = height;
    ctx.viewport.topLeftX = 0;
    ctx.viewport.topLeftY = 0;
    ctx.viewport.minDepth = 0.0f;
    ctx.viewport.maxDepth = 1.0f;

    // Calculate inverse view-projection for original camera (needed for reprojection shader)
    ctx.inverseViewProjection = InverseMatrix(camera.vp);

    return ctx;
}

} // namespace vrinject
