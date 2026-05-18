half4 main(float2 coords) {
    float2 p = (coords - 128.0) / 128.0;
    float angle = atan(p.y, p.x);
    float radius = saturate(length(p));

    half3 hue = half3(0.5 + 0.5 * cos(float3(0.0, 2.0943951, 4.1887902) + angle));
    half3 rgb = mix(half3(1.0), hue, half(radius));
    return half4(rgb, 1.0);
}
