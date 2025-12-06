// Simple fullscreen quad vertex shader for fill-blend test (KSL style)
in float2 in_position;
in float2 in_texcoord0_;

out float2 texcoord;

void main()
{
    texcoord = in_texcoord0_;
#ifdef NGL_ORIGIN_UPPER_LEFT_AND_NDC_FLIP
    texcoord.y = 1.0 - texcoord.y;
#endif
    gl_Position = float4(in_position, 0.0, 1.0);
}
