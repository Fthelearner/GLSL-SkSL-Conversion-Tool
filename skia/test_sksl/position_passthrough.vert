layout(location=0) in float2 pos;
layout(location=1) in half4 color;

layout(location=0) out half4 vColor;

void main() {
    vColor = color;
    sk_Position = float4(pos, 0.0, 1.0);
}
