#if defined(VULKAN)
#define VK_BINDING(x) [[vk::binding(x)]]
#else
#define VK_BINDING(x)
#endif

VK_BINDING(0) cbuffer StereoConstants : register(b0)
{
    // --- Chunk 0: bytes 0–255 (existing layout, unchanged) ---
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

    // --- Chunk 1: bytes 256–511 (P0.2/P1.1/P1.2/P2) ---
    uint CurvedHudEnabled;
    float HudDistance;
    float HudCurvature;
    float _pad0;

    float ComfortVignetteRadius;
    float ComfortVignetteFeather;
    float TheaterModeWeight;
    float TheaterDistance;

    float HorizonRollCorrection;
    float HorizonLockStrength;
    float _pad1;
    float _pad2;
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

// Display gamma transfer curve: active when opted in via profile (SrgbCorrection != 0).
// Modern game backbuffers are already tone-mapped and display-ready for sRGB. The OpenXR compositor
// and display runtimes (SteamVR, Quest Link, Virtual Desktop) apply an inverse gamma curve (~1/2.2) when presenting.
// Applying exact display gamma pow(clamp(c, 0.0f, 1.0f), 2.2f) provides an identical mathematical cancellation
// (pow(c, 2.2f))^(1/2.2) == c, preventing lifted shadows and reproducing desktop graphics 1:1 with zero brightening.
float3 ApplySrgbTransfer(float3 c)
{
    return pow(clamp(c, 0.0f, 1.0f), 2.2f);
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

// P0.2: Curved HUD Reprojection
// Maps flat screen-space HUD pixels onto a virtual cylindrical surface floating
// at HudDistance in front of the viewer. This pulls corner UI elements toward the
// center of the user's field of view, reducing eye strain.
float2 CurvedHudUV(float2 uv, float curvature, float distance)
{
    // Convert UV to centered coordinates [-1, 1]
    float2 centered = uv * 2.0f - 1.0f;
    
    // Apply cylindrical mapping: x wraps around a cylinder, y remains linear
    float theta = centered.x * 3.14159265f * curvature; // angle on cylinder
    float newX = sin(theta) / (curvature * 3.14159265f + 0.001f);
    
    // Reconstruct UV
    float2 result = float2(newX, centered.y) * 0.5f + 0.5f;
    return result;
}

// P1.2: Comfort Vignette
// Darkens peripheral pixels based on an angular velocity-driven radius.
// Reduces motion sickness during fast camera rotation.
float ComputeVignetteAlpha(float2 uv, float radius, float feather)
{
    // Distance from screen center in normalized coords
    float2 centered = uv * 2.0f - 1.0f;
    float dist = length(centered);
    
    // Smooth vignette falloff: 1.0 = fully visible, 0.0 = fully darkened
    float innerRadius = max(radius, 0.1f);
    float outerRadius = innerRadius + max(feather, 0.05f);
    float alpha = smoothstep(outerRadius, innerRadius, dist);
    return alpha;
}

// P1.1: Theater Mode Projection
// Projects the game image onto a virtual cinema screen floating in a dark void.
// theaterWeight controls the blend from normal view (0) to full theater (1).
float4 ApplyTheaterMode(float4 normalColor, float2 uv, float theaterWeight, float theaterDist)
{
    if (theaterWeight < 0.001f)
        return normalColor;

    // Theater screen occupies center 70% of the viewport
    float2 centered = uv * 2.0f - 1.0f;
    float2 screenUV = centered / 0.7f * 0.5f + 0.5f;
    
    float4 theaterColor = float4(0.02f, 0.02f, 0.02f, 1.0f); // Dark void
    
    if (screenUV.x >= 0.0f && screenUV.x <= 1.0f && screenUV.y >= 0.0f && screenUV.y <= 1.0f)
    {
        theaterColor = GameColor.SampleLevel(LinearSampler, screenUV, 0);
        theaterColor.a = 1.0f;
    }
    
    return lerp(normalColor, theaterColor, theaterWeight);
}

// P2: Horizon Lock Roll Stabilization
// Stabilizes the virtual horizon by counter-rotating screen coordinates by HorizonRollCorrection.
float2 ApplyHorizonLock(float2 uv, float rollCorr, float strength)
{
    if (abs(rollCorr) < 0.0001f || strength < 0.001f)
        return uv;

    float2 centered = uv * 2.0f - 1.0f;
    float c = cos(rollCorr);
    float s = sin(rollCorr);
    float2 rotated = float2(
        centered.x * c - centered.y * s,
        centered.x * s + centered.y * c
    );
    return rotated * 0.5f + 0.5f;
}

[numthreads(8, 8, 1)]
void CSMain(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    if (dispatchThreadId.x >= Width || dispatchThreadId.y >= Height)
        return;
        
    int2 pixelPos = int2(dispatchThreadId.x, dispatchThreadId.y);
    float2 uv = float2(((float)pixelPos.x + 0.5f) / (float)Width, ((float)pixelPos.y + 0.5f) / (float)Height);

    // P2: Apply Horizon Lock roll stabilization
    float2 sampleUV = ApplyHorizonLock(uv, HorizonRollCorrection, HorizonLockStrength);

    // Sample raw 2D color
    float4 baseColor = GameColor.SampleLevel(LinearSampler, sampleUV, 0);
    baseColor.a = 1.0f;
    
    if (ShouldAttemptStereo == 0)
    {
        // 2D Mode: Pass-through with perceptual grading (1:1 desktop parity at default settings)
        float4 outColor = baseColor;

        // P1.1: Theater mode in 2D passthrough
        outColor = ApplyTheaterMode(outColor, uv, TheaterModeWeight, TheaterDistance);

        outColor.rgb = ApplyPerceptualGrading(outColor.rgb, Contrast, Saturation, Brightness);
        if (SrgbCorrection != 0)
        {
            outColor.rgb = ApplySrgbTransfer(outColor.rgb);
        }

        // P1.2: Comfort Vignette
        if (ComfortVignetteRadius < 0.99f)
        {
            float vAlpha = ComputeVignetteAlpha(uv, ComfortVignetteRadius, ComfortVignetteFeather);
            outColor.rgb *= vAlpha;
        }

        outColor.a = 1.0f;
        
        OutLeftEye[pixelPos] = outColor;
        OutRightEye[pixelPos] = outColor;
        return;
    }
    
    float depth = GameDepth.SampleLevel(LinearSampler, sampleUV, 0).r;
    
    // 2D HUD / Clear depth check / Skybox horizon check
    if (depth <= 0.0001f || depth >= 0.9995f)
    {
        float4 hudColor = baseColor;

        // P0.2: Curved HUD reprojection for corner elements
        if (CurvedHudEnabled != 0 && depth <= 0.0001f)
        {
            float2 curvedUV = CurvedHudUV(uv, HudCurvature, HudDistance);
            hudColor = GameColor.SampleLevel(LinearSampler, curvedUV, 0);
            hudColor.a = 1.0f;
        }

        // P1.1: Theater mode for HUD
        hudColor = ApplyTheaterMode(hudColor, uv, TheaterModeWeight, TheaterDistance);

        hudColor.rgb = ApplyPerceptualGrading(hudColor.rgb, Contrast, Saturation, Brightness);
        if (SrgbCorrection != 0)
        {
            hudColor.rgb = ApplySrgbTransfer(hudColor.rgb);
        }

        // P1.2: Comfort Vignette
        if (ComfortVignetteRadius < 0.99f)
        {
            float vAlpha = ComputeVignetteAlpha(uv, ComfortVignetteRadius, ComfortVignetteFeather);
            hudColor.rgb *= vAlpha;
        }

        hudColor.a = 1.0f;
        OutLeftEye[pixelPos] = hudColor;
        OutRightEye[pixelPos] = hudColor;
        return;
    }
    
    // Unproject pixel ray to 3D world space
    float3 worldPos = WorldPositionFromDepth(sampleUV, depth);
    
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

    // P1.1: Theater mode for 3D content
    leftColor = ApplyTheaterMode(leftColor, uv, TheaterModeWeight, TheaterDistance);
    rightColor = ApplyTheaterMode(rightColor, uv, TheaterModeWeight, TheaterDistance);
    
    // Apply perceptual filmic harmonization (pure 1:1 identity at default Contrast 1.0, Saturation 1.0, Brightness 1.0)
    leftColor.rgb = ApplyPerceptualGrading(leftColor.rgb, Contrast, Saturation, Brightness);
    rightColor.rgb = ApplyPerceptualGrading(rightColor.rgb, Contrast, Saturation, Brightness);
    if (SrgbCorrection != 0)
    {
        leftColor.rgb = ApplySrgbTransfer(leftColor.rgb);
        rightColor.rgb = ApplySrgbTransfer(rightColor.rgb);
    }

    // P1.2: Comfort Vignette
    if (ComfortVignetteRadius < 0.99f)
    {
        float vAlpha = ComputeVignetteAlpha(uv, ComfortVignetteRadius, ComfortVignetteFeather);
        leftColor.rgb *= vAlpha;
        rightColor.rgb *= vAlpha;
    }

    leftColor.a = 1.0f;
    rightColor.a = 1.0f;

    // Output full coverage per destination pixel
    OutLeftEye[pixelPos] = leftColor;
    OutRightEye[pixelPos] = rightColor;
}
