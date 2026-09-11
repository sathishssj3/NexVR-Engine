// comfort_guard_analysis.hlsl
// GPU-side depth distribution analysis compute shader
// Aggregates depth values into a 16-bin logarithmic histogram

Texture2D<float> DepthBuffer : register(t0);

// 16-bin global structured buffer
RWStructuredBuffer<uint> GlobalHistogram : register(u0);

cbuffer StereoParams : register(b0) {
    float IPD;            // Interpupillary distance in meters
    float FocalLength;    // Focal length (derived from projection matrix)
    float NearPlane;      // Near clip plane
    float FarPlane;       // Far clip plane
    float Convergence;    // Distance where parallax is 0 (screen depth)
    float DepthStrength;  // Multiplier for depth effect
    uint Width;           // Texture width
    uint Height;          // Texture height
    uint ReversedZ;       // 1 if reversed-Z, 0 otherwise
    float3 _Padding;
};

[numthreads(16, 16, 1)]
void CSMain(uint3 id : SV_DispatchThreadID) {
    // We subsample the depth buffer using a stride of 8 to minimize GPU overhead
    uint sampleX = id.x * 8;
    uint sampleY = id.y * 8;

    // Bounds check
    if (sampleX >= Width || sampleY >= Height) {
        return;
    }

    // Sample raw depth
    float rawDepth = DepthBuffer[int2(sampleX, sampleY)];
    
    // Handle reversed-Z (1.0 at near plane, 0.0 at far plane)
    if (ReversedZ == 1) {
        rawDepth = 1.0f - rawDepth;
    }

    // Linearize depth
    float linearZ = (NearPlane * FarPlane) / (FarPlane - rawDepth * (FarPlane - NearPlane));
    linearZ = max(linearZ, NearPlane);

    // Map linearZ to 16 logarithmic bins
    // Near depth gets much higher resolution (bins 0-7 cover closest elements)
    float ratio = linearZ / NearPlane;
    float logRatio = log2(max(ratio, 1.0f));
    float maxLog = log2(max(FarPlane / NearPlane, 1.01f));
    
    uint bin = clamp((uint)((logRatio / maxLog) * 16.0f), 0, 15);

    // Atomically increment the corresponding bin in the global histogram
    InterlockedAdd(GlobalHistogram[bin], 1);
}
