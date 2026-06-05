// =============================================================================
// disocclusion_fill.hlsl — Pass 3: Horizontal Edge-Stretch Fill
// =============================================================================
//
// After stereo reprojection, the right-eye image contains "holes" — pixels
// where no source data exists because the original viewpoint couldn't see
// those regions (they were behind foreground objects).
//
// These disocclusions always appear on the LEFT side of foreground objects
// when reprojecting for the right eye, because the right eye's lateral offset
// reveals geometry that was hidden to the left of silhouette edges.
//
// Filling strategy: HORIZONTAL EDGE-STRETCHING
// ─────────────────────────────────────────────
// We scan each row left-to-right, carrying the last valid pixel color forward.
// When we encounter a hole (alpha == 0), we fill it with the carried color.
// This effectively "stretches" the edge of the background into the gap.
//
// Thread organization:
//   - Thread group: [16, 16, 1] — full 2D thread group parallelism
//   - Dispatch: (ceil(Width/16), ceil(Height/16), 1)
//   - Each thread handles ONE pixel. If it's a hole, it scans left up to
//     256 pixels to find the nearest valid pixel, avoiding serialization across
//     the entire 1920-pixel width.
// =============================================================================

// -- In-place read/write on the right eye texture ----------------------------
RWTexture2D<float4> RightEye : register(u0);

// -- Constants ---------------------------------------------------------------
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

// =============================================================================
// Main compute shader
// =============================================================================
[numthreads(16, 16, 1)]
void CSMain(uint3 DTid : SV_DispatchThreadID) {
    uint x = DTid.x;
    uint y = DTid.y;
    
    if (x >= Width || y >= Height) {
        return;
    }
    
    float4 pixel = RightEye[int2(x, y)];
    
    // If this pixel already has valid data (alpha > 0), we don't need to fill it
    if (pixel.a > 0.0) {
        return;
    }
    
    // We are at a hole. Scan left up to a reasonable distance (e.g. 256 pixels)
    // to find a valid background pixel to stretch.
    float4 fill_color = float4(0.0, 0.0, 0.0, 0.0);
    bool found = false;
    
    for (int i = 1; i <= 256 && (int)x - i >= 0; i++) {
        float4 search_pixel = RightEye[int2(x - i, y)];
        if (search_pixel.a > 0.0) {
            // Found a valid original pixel! 
            // We write the color but KEEP alpha = 0.0 so we don't accidentally
            // cascade invalid data into other threads running in parallel.
            fill_color = float4(search_pixel.rgb, 0.0);
            found = true;
            break;
        }
    }
    
    // If not found scanning left, try scanning right (for extreme left screen edges)
    if (!found) {
        for (int i = 1; i <= 256 && x + i < Width; i++) {
            float4 search_pixel = RightEye[int2(x + i, y)];
            if (search_pixel.a > 0.0) {
                fill_color = float4(search_pixel.rgb, 0.0);
                found = true;
                break;
            }
        }
    }
    
    // Write the filled color (alpha remains 0 to mark it as filled data)
    RightEye[int2(x, y)] = fill_color;
}
