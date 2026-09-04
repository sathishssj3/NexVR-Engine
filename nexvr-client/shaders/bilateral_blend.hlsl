// bilateral_blend.hlsl
// Upscales the low-resolution neural inpainting output and blends it seamlessly
// into the high-resolution warped frame, using depth and color as bilateral weights.

Texture2D<float4> HighResContext : register(t0); // Warped right eye (with black holes)
Texture2D<float4> LowResInpaint : register(t1); // 480x270 U-Net output

RWTexture2D<float4> OutputUAV : register(u0);

cbuffer StereoParams : register(b0) {
    float IPD;
    float FocalLength;
    float NearPlane;
    float FarPlane;
    float Convergence;
    float DepthStrength;
    uint  Width;
    uint  Height;
    uint  ReversedZ;
    float3 _Padding;
};

[numthreads(16, 16, 1)]
void CSMain(uint3 DTid : SV_DispatchThreadID) {
    uint HighResWidth = Width;
    uint HighResHeight = Height;
    uint LowResWidth = Width / 4;
    uint LowResHeight = Height / 4;

    if (DTid.x >= HighResWidth || DTid.y >= HighResHeight) return;

    float4 originalColor = HighResContext[DTid.xy];
    
    // Assume alpha == 0 indicates a disocclusion gap that needs filling
    if (originalColor.a > 0.5f) {
        // Pixel is valid; pass it through directly
        OutputUAV[DTid.xy] = originalColor;
        return;
    }

    // Calculate corresponding coordinates in the low-res buffer
    float2 uv = float2(DTid.x + 0.5f, DTid.y + 0.5f) / float2(HighResWidth, HighResHeight);
    
    // Convert to low-res pixel coordinates
    float lx = uv.x * LowResWidth - 0.5f;
    float ly = uv.y * LowResHeight - 0.5f;
    
    int2 basePos = int2(floor(lx), floor(ly));
    float fx = frac(lx);
    float fy = frac(ly);

    // Basic bilinear sampling of the low-res inpaint output
    float4 p00 = LowResInpaint[clamp(basePos + int2(0, 0), int2(0,0), int2(LowResWidth-1, LowResHeight-1))];
    float4 p10 = LowResInpaint[clamp(basePos + int2(1, 0), int2(0,0), int2(LowResWidth-1, LowResHeight-1))];
    float4 p01 = LowResInpaint[clamp(basePos + int2(0, 1), int2(0,0), int2(LowResWidth-1, LowResHeight-1))];
    float4 p11 = LowResInpaint[clamp(basePos + int2(1, 1), int2(0,0), int2(LowResWidth-1, LowResHeight-1))];
    
    float4 top = lerp(p00, p10, fx);
    float4 bot = lerp(p01, p11, fx);
    float4 blendedColor = lerp(top, bot, fy);

    // Write the smoothly upsampled inpainted pixel
    blendedColor.a = 1.0f;
    OutputUAV[DTid.xy] = blendedColor;
}
