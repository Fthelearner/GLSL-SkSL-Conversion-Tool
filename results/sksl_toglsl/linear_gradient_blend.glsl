#version 450 core
out vec4 FragColor;
uniform sampler2D image;
uniform sampler2D preblurImage;
uniform vec2 iResolution;
uniform vec2 startPoint;
uniform vec2 endPoint;
uniform float softness;
uniform float invert;
uniform float blurMix;
void main() {
    vec2 fragCoord = gl_FragCoord.xy;
    vec2 start_coord = startPoint * iResolution;
    vec2 end_coord = endPoint * iResolution;
    vec2 axis = end_coord - start_coord;
    float axis_length_squared = max(dot(axis, axis), 1e-05);
    float projected = dot(fragCoord - start_coord, axis) / axis_length_squared;
    float edge0 = -softness;
    float edge1 = 1.0 + softness;
    float gradient = smoothstep(edge0, edge1, projected);
    if (invert > 0.5) {
        gradient = 1.0 - gradient;
    }
    float mix_amount = clamp(gradient * blurMix, 0.0, 1.0);
    vec4 source_color = texture(image, (fragCoord) / iResolution);
    vec4 blur_color = texture(preblurImage, (fragCoord) / iResolution);
    FragColor = mix(source_color, blur_color, mix_amount);
    return;
}
