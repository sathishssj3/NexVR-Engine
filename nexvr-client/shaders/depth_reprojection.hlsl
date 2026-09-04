cbuffer ReprojectionConstants : register(b0)
{
    float IPD;
    float Convergence;
    float NearPlane;
    float FarPlane;
    float Width;
    float Height;
    float ReversedZ;
    float Padding;
};

Texture2D<float4> LeftEyeColor : register(t0);
Texture2D<float> LeftEyeDepth : register(t1);

RWTexture2D<float4> RightEyeColor : register(u0);

SamplerState PointSampler : register(s0);

// Convert depth buffer value to linear depth
float LinearizeDepth(float depth)
{
    if (ReversedZ > 0.5f) {
        // Reversed Z: near=1, far=0
        return (NearPlane * FarPlane) / (FarPlane - depth * (FarPlane - NearPlane));
    } else {
        // Standard Z: near=0, far=1
        return (NearPlane * FarPlane) / (NearPlane + depth * (FarPlane - NearPlane));
    }
}

[numthreads(16, 16, 1)]
void CSMain(uint3 DTid : SV_DispatchThreadID)
{
    if (DTid.x >= (uint)Width || DTid.y >= (uint)Height)
        return;

    // Default to left eye pixel if no depth is available
    float4 color = LeftEyeColor.Load(int3(DTid.xy, 0));
    float depth = LeftEyeDepth.Load(int3(DTid.xy, 0));

    // Fallback if depth is out of bounds
    if (depth <= 0.0001f || depth >= 0.9999f) {
        RightEyeColor[DTid.xy] = color;
        return;
    }

    float linearDepth = LinearizeDepth(depth);

    // Calculate parallax shift for the right eye
    // Shift is based on IPD and the distance to the focal plane (Convergence)
    float shift = IPD * (1.0f - (Convergence / linearDepth));
    
    // Calculate new X coordinate in the right eye
    // Positive shift means moving pixels to the left in the right eye view
    int rightX = (int)((float)DTid.x - shift * Width * 0.5f); // Scale shift by screen width

    // Basic Bounds check and write
    if (rightX >= 0 && rightX < (int)Width) {
        // Use InterlockedMax or similar atomic operation to handle depth conflicts correctly
        // Since RWTexture2D doesn't support atomics natively without a typed UAV or specialized formats,
        // we'll use a basic write for prototyping. A more robust implementation would use a depth buffer.
        
        // As a simple heuristic, we write to the calculated coordinate. 
        // This causes artifacts where multiple pixels map to the same target (occlusion) 
        // and leaves gaps (disocclusion).
        RightEyeColor[int2(rightX, DTid.y)] = color;
    }

    // Fill in the original pixel too, in case of gaps (simple smear)
    // A proper inpainting pass will fix the gaps later.
    if (RightEyeColor[DTid.xy].a == 0) {
        RightEyeColor[DTid.xy] = color;
    }
}
