// =============================================================================
// stereo_resolve.hlsl — Pass 2: Resolve Warped Pixels to Right Eye
// =============================================================================
//
// After the warp pass, the WarpBuffer contains a packed uint per output pixel:
//   bits [31:16] = quantized depth of the nearest source pixel
//   bits [15:0]  = X coordinate of that source pixel in the original frame
//
// This pass "resolves" the indirection: for each right-eye output pixel, we
// look up which source pixel won the depth competition and copy its color.
//
// Pixels where no source pixel landed (WarpBuffer == 0xFFFFFFFF) are
// disocclusions — regions that were hidden behind foreground objects in the
// original view but are now visible from the right eye's offset position.
// We mark these with alpha = 0 so Pass 3 can identify and fill them.
// =============================================================================

// -- Inputs ------------------------------------------------------------------
Texture2D<float4> ColorBuffer : register(t0);   // Original game frame
Texture2D<uint>   WarpBuffer  : register(t1);   // Packed result from Pass 1 (as SRV now)

// -- Outputs -----------------------------------------------------------------
RWTexture2D<float4> RightEye : register(u0);    // Right eye color output

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
    uint outX = DTid.x;
    uint outY = DTid.y;
    
    if (outX >= Width || outY >= Height) {
        return;
    }
    
    // Read the packed depth+source value that won the atomicMin race
    uint packed = WarpBuffer[int2(outX, outY)];
    
    if (packed == 0xFFFFFFFF) {
        // -- Disocclusion: no source pixel mapped here --
        // This happens when the right-eye viewpoint reveals geometry that
        // was occluded in the original frame. There's simply no color data
        // available for this pixel from the original render.
        //
        // We write transparent black so Pass 3 (disocclusion fill) can
        // detect and fill these holes by examining alpha == 0.
        RightEye[int2(outX, outY)] = float4(0.0, 0.0, 0.0, 0.0);
        return;
    }
    
    // -- Unpack the winning source pixel's X coordinate --
    // The Y coordinate is preserved because DIBR with horizontal-only
    // camera offset produces only horizontal disparity (no vertical shift).
    uint sourceX = packed & 0xFFFF;
    
    // Safety clamp (should never be needed if warp pass bounds-checks correctly)
    sourceX = min(sourceX, Width - 1);
    
    // -- Copy the source pixel's color to the right eye output --
    float4 color = ColorBuffer[int2(sourceX, outY)];
    
    // Force alpha to 1.0 to mark this pixel as valid/filled.
    // This is critical for Pass 3, which uses alpha == 0 to find holes.
    RightEye[int2(outX, outY)] = float4(color.rgb, 1.0);
}
