#if defined(VULKAN)
#define VK_BINDING(x) [[vk::binding(x)]]
#else
#define VK_BINDING(x)
#endif

VK_BINDING(0) cbuffer StereoConstants : register(b0)
{
    row_major float4x4 InverseViewProj;
    row_major float4x4 LeftViewProj;
    row_major float4x4 RightViewProj;
    
    float3 OriginalEyePos;
    float Contrast;
    
    float3 LeftEyePos;
    float Saturation;
    
    float3 RightEyePos;
    float Brightness;
    
    uint Width;
    uint Height;
    uint ShouldAttemptStereo;
    uint SrgbCorrection;
};

VK_BINDING(1) Texture2D<float4> GameColor : register(t0);
VK_BINDING(2) Texture2D<float> GameDepth : register(t1);
VK_BINDING(3) SamplerState LinearSampler : register(s0);

VK_BINDING(4) RWTexture2D<float4> OutLeftEye : register(u0);
VK_BINDING(5) RWTexture2D<float4> OutRightEye : register(u1);

// Perceptually calibrated contrast and saturation adjustment.
// Prevents crushed blacks, lifts flat midtones, and preserves highlight detail.
// Eliminates crushed blacks while restoring the true warmth and lighting of desktop monitors.
float3 ApplyPerceptualGrading(float3 color, float contrast, float saturation, float brightness)
{
    float c = (contrast > 0.01f) ? contrast : 1.0f;
    float s = (saturation > 0.01f) ? saturation : 1.0f;
    float b = (brightness > 0.01f) ? brightness : 1.0f;

    // Fast-path: Identity (default 1.0 for all games without specific grading)
    if (abs(c - 1.0f) < 0.001f && abs(s - 1.0f) < 0.001f && abs(b - 1.0f) < 0.001f)
    {
        return color;
    }

    float3 col = max(color, 0.0f);

    // 1. Exposure gain
    col *= b;

    // 2. Perceptual Filmic Contrast Curve with protected shadow toe
    // Unlike a raw linear pivot which plunges 0.1 shadows to 0.02 pitch black,
    // this spline lifts the toe in dark areas so atmospheric lighting, walls,
    // and ground detail remain rich and visible just like on the desktop monitor.
    if (abs(c - 1.0f) > 0.001f)
    {
        float3 toe = pow(col, 1.0f / max(c, 0.1f));
        float3 shoulder = 1.0f - pow(max(1.0f - col, 0.0f), c);
        float3 weight = smoothstep(0.12f, 0.88f, col);
        col = lerp(toe, shoulder, weight);
    }

    // 3. Rec.709 Luminance-preserving chroma adjustment
    float lum = dot(col, float3(0.2126f, 0.7152f, 0.0722f));
    col = lerp(float3(lum, lum, lum), col, s);

    return saturate(col);
}

// Optional IEC 61966-2-1 transfer curve: active ONLY if explicitly opted in via profile (SrgbCorrection != 0).
// Modern game backbuffers are already tone-mapped and display-ready for sRGB; default passes through 1:1 untouched.
float3 ApplySrgbTransfer(float3 c)
{
    float3 linearLow = c / 12.92f;
    float3 linearHigh = pow(max((c + 0.055f) / 1.055f, 0.0f), 2.4f);
    return lerp(linearLow, linearHigh, step(0.04045f, c));
}

// Standard depth unprojection
float3 WorldPositionFromDepth(float2 uv, float depth)
{
    float x = uv.x * 2.0f - 1.0f;
    float y = (1.0f - uv.y) * 2.0f - 1.0f;
    float4 clipSpacePosition = float4(x, y, depth, 1.0f);
    
    float4 worldSpacePosition = mul(clipSpacePosition, InverseViewProj);
    return worldSpacePosition.xyz / worldSpacePosition.w;
}

// 8-tap bilateral edge-aware push-pull hole inpainter for disoccluded stereo regions
float4 InpaintDisocclusion(int2 p, float refDepth, float4 fallbackColor, uint w, uint h)
{
    float4 sumColor = float4(0, 0, 0, 0);
    float totalWeight = 0.0001f;
    
    const int2 offsets[8] = {
        int2(-1, -1), int2(0, -1), int2(1, -1),
        int2(-1,  0),              int2(1,  0),
        int2(-1,  1), int2(0,  1), int2(1,  1)
    };
    
    [unroll]
    for (int i = 0; i < 8; ++i)
    {
        int2 neighborPos = clamp(p + offsets[i] * 2, int2(0, 0), int2(w - 1, h - 1));
        float2 neighborUV = float2(((float)neighborPos.x + 0.5f) / (float)w, ((float)neighborPos.y + 0.5f) / (float)h);
        float neighborDepth = GameDepth.Load(int3(neighborPos, 0));
        float4 neighborColor = GameColor.SampleLevel(LinearSampler, neighborUV, 0);
        
        // Prefer deeper background pixels to fill disocclusions cleanly
        float depthWeight = (neighborDepth >= refDepth) ? 2.0f : 0.5f;
        sumColor += neighborColor * depthWeight;
        totalWeight += depthWeight;
    }
    
    if (totalWeight < 0.01f)
    {
        return fallbackColor;
    }

    float4 result = sumColor / totalWeight;
    result.a = 1.0f;
    return result;
}

[numthreads(8, 8, 1)]
void CSMain(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    if (dispatchThreadId.x >= Width || dispatchThreadId.y >= Height)
        return;
        
    int2 pixelPos = int2(dispatchThreadId.x, dispatchThreadId.y);
    float2 uv = float2(((float)pixelPos.x + 0.5f) / (float)Width, ((float)pixelPos.y + 0.5f) / (float)Height);

    // Sample raw 2D color
    float4 baseColor = GameColor.SampleLevel(LinearSampler, uv, 0);
    baseColor.a = 1.0f;
    
    if (ShouldAttemptStereo == 0)
    {
        // 2D Mode: Pass-through with perceptual grading (1:1 desktop parity at default settings)
        float4 outColor = baseColor;
        outColor.rgb = ApplyPerceptualGrading(outColor.rgb, Contrast, Saturation, Brightness);
        if (SrgbCorrection != 0)
        {
            outColor.rgb = ApplySrgbTransfer(outColor.rgb);
        }
        outColor.a = 1.0f;
        
        OutLeftEye[pixelPos] = outColor;
        OutRightEye[pixelPos] = outColor;
        return;
    }
    
    float depth = GameDepth.SampleLevel(LinearSampler, uv, 0).r;
    
    // 2D HUD / Clear depth check / Skybox horizon check
    if (depth <= 0.0001f || depth >= 0.9995f)
    {
        float4 hudColor = baseColor;
        hudColor.rgb = ApplyPerceptualGrading(hudColor.rgb, Contrast, Saturation, Brightness);
        if (SrgbCorrection != 0)
        {
            hudColor.rgb = ApplySrgbTransfer(hudColor.rgb);
        }
        hudColor.a = 1.0f;
        OutLeftEye[pixelPos] = hudColor;
        OutRightEye[pixelPos] = hudColor;
        return;
    }
    
    // Unproject pixel ray to 3D world space
    float3 worldPos = WorldPositionFromDepth(uv, depth);
    
    // Left Eye Backward Gather
    float4 leftClip = mul(float4(worldPos, 1.0f), LeftViewProj);
    float4 leftColor = baseColor;
    if (leftClip.w > 0.0001f)
    {
        float2 leftNdc = leftClip.xy / leftClip.w;
        float2 leftUV = float2(leftNdc.x * 0.5f + 0.5f, 1.0f - (leftNdc.y * 0.5f + 0.5f));
        
        if (leftUV.x >= 0.0f && leftUV.x <= 1.0f && leftUV.y >= 0.0f && leftUV.y <= 1.0f)
        {
            leftColor = GameColor.SampleLevel(LinearSampler, leftUV, 0);
            leftColor.a = 1.0f;
        }
        else
        {
            leftColor = InpaintDisocclusion(pixelPos, depth, baseColor, Width, Height);
        }
    }
    
    // Right Eye Backward Gather
    float4 rightClip = mul(float4(worldPos, 1.0f), RightViewProj);
    float4 rightColor = baseColor;
    if (rightClip.w > 0.0001f)
    {
        float2 rightNdc = rightClip.xy / rightClip.w;
        float2 rightUV = float2(rightNdc.x * 0.5f + 0.5f, 1.0f - (rightNdc.y * 0.5f + 0.5f));
        
        if (rightUV.x >= 0.0f && rightUV.x <= 1.0f && rightUV.y >= 0.0f && rightUV.y <= 1.0f)
        {
            rightColor = GameColor.SampleLevel(LinearSampler, rightUV, 0);
            rightColor.a = 1.0f;
        }
        else
        {
            rightColor = InpaintDisocclusion(pixelPos, depth, baseColor, Width, Height);
        }
    }
    
    // Apply perceptual filmic harmonization (pure 1:1 identity at default Contrast 1.0, Saturation 1.0, Brightness 1.0)
    leftColor.rgb = ApplyPerceptualGrading(leftColor.rgb, Contrast, Saturation, Brightness);
    rightColor.rgb = ApplyPerceptualGrading(rightColor.rgb, Contrast, Saturation, Brightness);
    if (SrgbCorrection != 0)
    {
        leftColor.rgb = ApplySrgbTransfer(leftColor.rgb);
        rightColor.rgb = ApplySrgbTransfer(rightColor.rgb);
    }
    leftColor.a = 1.0f;
    rightColor.a = 1.0f;

    // Output full coverage per destination pixel
    OutLeftEye[pixelPos] = leftColor;
    OutRightEye[pixelPos] = rightColor;
}
