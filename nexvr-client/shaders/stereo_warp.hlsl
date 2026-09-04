// stereo_warp.hlsl
// Compute shader for Depth-Image-Based Rendering (DIBR) - Pass 1
// Warps source pixels to the right eye view based on depth

Texture2D<float4> ColorBuffer : register(t0);
Texture2D<float> DepthBuffer  : register(t1);

// Packed format: (depth_16bit << 16) | source_x
RWTexture2D<uint> WarpBuffer  : register(u0);

cbuffer StereoParams : register(b0) {
    float IPD;            // Interpupillary distance in meters (e.g. 0.063)
    float FocalLength;    // Focal length (derived from projection matrix)
    float NearPlane;      // Near clip plane (e.g. 0.1)
    float FarPlane;       // Far clip plane (e.g. 1000.0)
    float Convergence;    // Distance where parallax is 0 (screen depth)
    float DepthStrength;  // Multiplier for depth effect (default 1.0)
    uint Width;           // Texture width
    uint Height;          // Texture height
    uint ReversedZ;       // 1 if reversed-Z, 0 otherwise
    float3 _Padding;
};

[numthreads(16, 16, 1)]
void CSMain(uint3 id : SV_DispatchThreadID) {
    uint x = id.x;
    uint y = id.y;

    // Bounds check
    if (x >= Width || y >= Height) {
        return;
    }

    // Sample raw depth
    float rawDepth = DepthBuffer[int2(x, y)];
    
    // Handle reversed-Z (1.0 at near plane, 0.0 at far plane)
    if (ReversedZ == 1) {
        rawDepth = 1.0f - rawDepth;
    }

    // Linearize depth
    // Standard perspective projection linearization
    float linearZ = (NearPlane * FarPlane) / (FarPlane - rawDepth * (FarPlane - NearPlane));
    
    // Clamp to avoid division by zero or extreme parallax for skyboxes
    linearZ = max(linearZ, NearPlane);

    // Compute parallax shift for this depth
    // shift = (focal_length * baseline) / Z
    float shift = (FocalLength * IPD * DepthStrength) / linearZ;
    
    // Compute convergence shift (parallax at the convergence plane)
    float convergenceShift = (FocalLength * IPD * DepthStrength) / Convergence;
    
    // Final pixel shift (relative to screen center)
    // We shift the right eye leftwards relative to objects that are closer than convergence
    float pixelShift = shift - convergenceShift;
    
    // Right eye shifts left for closer objects
    int target_x = (int)x - (int)(pixelShift * 0.5f);

    // Check if target pixel is on screen
    if (target_x >= 0 && target_x < (int)Width) {
        // Quantize depth for atomic sorting (closer objects win)
        // Map [NearPlane, FarPlane] to [0, 65535]
        float depthNormalized = saturate(linearZ / FarPlane);
        uint depthQ = (uint)(depthNormalized * 65535.0f);
        
        // Pack 16-bit depth and 16-bit source X coordinate
        uint packed = (depthQ << 16) | (x & 0xFFFF);
        
        // Atomic min ensures that pixels with smaller depth (closer to camera) overwrite background pixels
        InterlockedMin(WarpBuffer[int2(target_x, y)], packed);
    }
}
