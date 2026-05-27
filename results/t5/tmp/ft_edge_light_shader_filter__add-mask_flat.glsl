#version 450 core
out vec4 FragColor;
uniform sampler2D imageMask;
uniform sampler2D image;
uniform sampler2D imageBloom;
uniform vec2 iResolution;
uniform float alphaProgress;
void main() {
    vec2 fragCoord = gl_FragCoord.xy;
    vec4 image_color = texture(image, (fragCoord) / iResolution);
    image_color.rgb *= image_color.a;
    float mask = texture(imageMask, (fragCoord) / iResolution).w;
    vec4 final_color = image_color;
    vec4 dst = vec4(1.0, 1.0, iResolution.x - 1.0, iResolution.y - 1.0);
    if ((((dst.x < fragCoord.x && fragCoord.x < dst.z) && dst.y < fragCoord.y) && fragCoord.y < dst.w) && mask > 0.0) {
        final_color = vec4((alphaProgress * texture(imageBloom, (fragCoord) / iResolution).xyz) * mask + image_color.xyz, image_color.w);
    }
    FragColor = final_color;
    FragColor = vec4(FragColor.xyz, 1.0);
    return;
}
