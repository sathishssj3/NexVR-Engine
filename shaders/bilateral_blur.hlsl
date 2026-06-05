// =============================================================================
// bilateral_blur.hlsl — Pass 4: Edge-Preserving Blur for Disocclusions
// =============================================================================
//
// This pass smooths out the horizontal streaks created by the fill pass.
// It selectively targets ONLY the filled pixels (identified by alpha == 0.0).
//
// It uses a Color-Based Bilateral Filter. By weighting neighboring pixels
// based on their color similarity, it blurs the background streaks together
// while ignoring the sharp, high-contrast foreground edges.
// =============================================================================

Texture2D<float4> InputTex : register(t0);
RWTexture2D<float4> OutputTex : register(u0);

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

// No hardcoded spatial weights needed, we calculate dynamically
[numthreads(16, 16, 1)]
void CSMain(uint3 DTid : SV_DispatchThreadID) {
    uint x = DTid.x;
    uint y = DTid.y;
    
    if (x >= Width || y >= Height) return;
    
    float4 centerPixel = InputTex[int2(x, y)];
    
    // If it's an original valid pixel (alpha > 0), don't blur it! 
    if (centerPixel.a > 0.0) {
        OutputTex[int2(x, y)] = float4(centerPixel.rgb, 1.0);
        return;
    }
    
    float3 colorSum = float3(0, 0, 0);
    float weightSum = 0.0;
    
    // Increased Sigmas for a much smoother, softer blur
    float spatialSigma = 3.0; 
    float colorSigma = 0.25; 
    
    // 11x11 Neighborhood (Radius = 5)
    for (int dy = -5; dy <= 5; ++dy) {
        for (int dx = -5; dx <= 5; ++dx) {
            int2 neighborPos = int2(x + dx, y + dy);
            
            // Clamp to screen bounds
            neighborPos.x = clamp(neighborPos.x, 0, Width - 1);
            neighborPos.y = clamp(neighborPos.y, 0, Height - 1);
            
            float4 neighborPixel = InputTex[neighborPos];
            
            // Spatial weight (Dynamic Gaussian)
            float spatialDistSq = (float)(dx*dx + dy*dy);
            float spatialWeight = exp(-spatialDistSq / (2.0 * spatialSigma * spatialSigma));
            
            // Color weight (Gaussian based on color difference)
            float colorDiffSq = dot(centerPixel.rgb - neighborPixel.rgb, centerPixel.rgb - neighborPixel.rgb);
            float colorWeight = exp(-colorDiffSq / (2.0 * colorSigma * colorSigma));
            
            float totalWeight = spatialWeight * colorWeight;
            
            colorSum += neighborPixel.rgb * totalWeight;
            weightSum += totalWeight;
        }
    }
    
    float3 finalColor = colorSum / max(weightSum, 0.0001); // Prevent div by 0
    
    OutputTex[int2(x, y)] = float4(finalColor, 1.0);
}
