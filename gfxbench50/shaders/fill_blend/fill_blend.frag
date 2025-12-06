// Simple white output with alpha 1.0; relies on blend state from pipeline.
out float4 out_color { color(0) };

void main()
{
    out_color = float4(1.0, 1.0, 1.0, 1.0);
}
