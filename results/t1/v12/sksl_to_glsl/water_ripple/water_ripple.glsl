#version 450 core
out vec4 FragColor;
uniform sampler2D image;
uniform vec2 iResolution;
uniform float progress;
uniform float waveCount;
uniform vec2 rippleCenter;
float calc_wave_ff_ff(float distance_to_wave) {
    float pre_wave = smoothstep(0.0, -0.3, distance_to_wave);
    float wave_form = waveCount == 1.0 ? smoothstep(-0.4, -0.2, distance_to_wave) * smoothstep(0.0, -0.2, distance_to_wave) : (waveCount == 2.0 ? smoothstep(-0.6, -0.3, distance_to_wave) * pre_wave : (smoothstep(-0.9, -0.6, distance_to_wave) * step(abs(distance_to_wave + 0.45), 0.45)) * pre_wave);
    return -sin(31.0 * distance_to_wave) * wave_form;
}
void main() {
    vec2 fragCoord = gl_FragCoord.xy;
    float shortEdge = min(iResolution.x, iResolution.y);
    vec2 uv = fragCoord / iResolution;
    vec2 uv_homogeneous = fragCoord / shortEdge;
    vec2 resolution_ratio = iResolution / shortEdge;
    float progress_slope = 0.5 + 0.1 * waveCount;
    float time_value = progress_slope * progress;
    vec2 wave_center = rippleCenter * resolution_ratio;
    float propagated_distance = distance(uv_homogeneous, wave_center);
    vec2 wave_vector = uv_homogeneous - wave_center;
    float amplitude_decay = propagated_distance < 1.0 ? pow(1.0 - propagated_distance, 4.0) : 0.0;
    float amplitude_suppress = smoothstep(0.0, 0.45, propagated_distance);
    float _0_distance_to_wave = propagated_distance - 2.0 * time_value;
    float _2_d1 = _0_distance_to_wave - 0.001;
    float _3_d2 = _0_distance_to_wave + 0.001;
    float intensity = ((((calc_wave_ff_ff(_3_d2) - calc_wave_ff_ff(_2_d1)) * 499.999969) * amplitude_decay) * amplitude_suppress) * 0.012;
    vec2 normalized_wave_vector = length(wave_vector) > 1e-05 ? normalize(wave_vector) : vec2(0.0);
    vec2 circles = normalized_wave_vector * intensity;
    vec3 normal = vec3(circles, intensity);
    vec2 distorted_coord = (uv - 0.15 * normal.xy) * iResolution;
    vec3 color = texture(image, (distorted_coord) / iResolution).xyz;
    color += 150.0 * pow(clamp(dot(normal, vec3(0.0, 0.992277861, -0.124034733)), 0.0, 1.0), 2.5);
    FragColor = vec4(color, 1.0);
    return;
}
