cbuffer StereoConstants : register(b0)
{
    row_major float4x4 InverseViewProj;
    row_major float4x4 LeftViewProj;
    row_major float4x4 RightViewProj;
    
    float3 OriginalEyePos;
    float Pad0;
    
    float3 LeftEyePos;
    float Pad1;
    
    float3 RightEyePos;
    float Pad2;
    
    uint Width;
    uint Height;
    float2 Padding;
};

Texture2D<float4> GameColor : register(t0);
Texture2D<float> GameDepth : register(t1);

RWTexture2D<float4> OutLeftEye : register(u0);
RWTexture2D<float4> OutRightEye : register(u1);

// Standard depth unprojection
float3 WorldPositionFromDepth(float2 uv, float depth)
{
    float x = uv.x * 2.0f - 1.0f;
    float y = (1.0f - uv.y) * 2.0f - 1.0f;
    float4 clipSpacePosition = float4(x, y, depth, 1.0f);
    
    float4 worldSpacePosition = mul(clipSpacePosition, InverseViewProj);
    return worldSpacePosition.xyz / worldSpacePosition.w;
}

[numthreads(8, 8, 1)]
void CSMain(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    if (dispatchThreadId.x >= Width || dispatchThreadId.y >= Height)
        return;
        
    int2 pixelPos = int2(dispatchThreadId.x, dispatchThreadId.y);
    float2 uv = float2((float)pixelPos.x / Width, (float)pixelPos.y / Height);
    
    float depth = GameDepth.Load(int3(pixelPos, 0));
    float4 color = GameColor.Load(int3(pixelPos, 0));
    
    // Default fallback to center eye (can be improved with scattering/gathering)
    OutLeftEye[pixelPos] = color;
    OutRightEye[pixelPos] = color;
    
    // Only reproject valid depth
    // Usually depth == 1.0f or 0.0f means far plane/skybox.
    // For a basic forward projection, we just map everything.
    float3 worldPos = WorldPositionFromDepth(uv, depth);
    
    // Project into left eye
    float4 leftClip = mul(float4(worldPos, 1.0f), LeftViewProj);
    leftClip.xyz /= leftClip.w;
    int2 leftPixel = int2((leftClip.x * 0.5f + 0.5f) * Width, (1.0f - (leftClip.y * 0.5f + 0.5f)) * Height);
    
    if (leftPixel.x >= 0 && leftPixel.x < Width && leftPixel.y >= 0 && leftPixel.y < Height)
    {
        // InterlockedMin depth could be used if doing true forward scattering, 
        // but for Sprint 3.5 we'll just write directly to demonstrate the baseline works.
        // A full splatting approach requires atomics on a depth buffer or a gather approach.
        // For now, this validates the math.
        OutLeftEye[leftPixel] = color;
    }
    
    // Project into right eye
    float4 rightClip = mul(float4(worldPos, 1.0f), RightViewProj);
    rightClip.xyz /= rightClip.w;
    int2 rightPixel = int2((rightClip.x * 0.5f + 0.5f) * Width, (1.0f - (rightClip.y * 0.5f + 0.5f)) * Height);
    
    if (rightPixel.x >= 0 && rightPixel.x < Width && rightPixel.y >= 0 && rightPixel.y < Height)
    {
        OutRightEye[rightPixel] = color;
    }
}
