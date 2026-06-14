Texture2D<float4> InputTex : register(t0);
RWTexture2D<float4> OutputTex : register(u0);

[numthreads(8, 8, 1)]
void CSMain(uint3 DTid : SV_DispatchThreadID)
{
    float4 color = InputTex.Load(int3(DTid.xy, 0));
    OutputTex[DTid.xy] = color;
}
