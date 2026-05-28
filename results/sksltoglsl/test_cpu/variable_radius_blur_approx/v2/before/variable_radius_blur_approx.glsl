#version 450 core
out vec4 FragColor;
uniform sampler2D image;
uniform sampler2D blurMask;
uniform vec2 iResolution;
uniform float maxRadius;
uniform float strength;
vec4 sample_pair_h4f2f2fff(vec2 coord, vec2 direction, float radius, float factor, inout float totalWeight) {
    vec2 offset = (direction * radius) * factor;
    float weight = 1.0 - 0.18 * factor;
    totalWeight += 2.0 * weight;
    return (texture(image, (coord + offset) / iResolution) + texture(image, (coord - offset) / iResolution)) * weight;
}
void main() {
    vec2 coord = gl_FragCoord.xy;
    float maskAlpha = texture(blurMask, (coord) / iResolution).w;
    float radius = clamp((maskAlpha * maxRadius) * strength, 0.0, maxRadius);
    if (radius < 0.5) {
        FragColor = texture(image, (coord) / iResolution);
        return;
    }
    vec4 sum = texture(image, (coord) / iResolution) * 1.4;
    float totalWeight = 1.4;
    vec2 horizontal = vec2(1.0, 0.0);
    vec2 vertical = vec2(0.0, 1.0);
    vec2 diagonalA = vec2(0.707106769);
    vec2 diagonalB = vec2(0.707106769, -0.707106769);
    sum += sample_pair_h4f2f2fff(coord, horizontal, radius, 0.33, totalWeight);
    sum += sample_pair_h4f2f2fff(coord, horizontal, radius, 0.66, totalWeight);
    sum += sample_pair_h4f2f2fff(coord, horizontal, radius, 1.0, totalWeight);
    sum += sample_pair_h4f2f2fff(coord, vertical, radius, 0.33, totalWeight);
    sum += sample_pair_h4f2f2fff(coord, vertical, radius, 0.66, totalWeight);
    sum += sample_pair_h4f2f2fff(coord, vertical, radius, 1.0, totalWeight);
    sum += sample_pair_h4f2f2fff(coord, diagonalA, radius, 0.5, totalWeight);
    sum += sample_pair_h4f2f2fff(coord, diagonalA, radius, 0.9, totalWeight);
    sum += sample_pair_h4f2f2fff(coord, diagonalB, radius, 0.5, totalWeight);
    sum += sample_pair_h4f2f2fff(coord, diagonalB, radius, 0.9, totalWeight);
    FragColor = sum / totalWeight;
    return;
}
