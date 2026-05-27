#version 450 core
out vec4 FragColor;
uniform sampler2D image;
uniform sampler2D composeImage;
void main() {
    vec2 fragCoord = gl_FragCoord.xy;
    vec4 imageColor = texture(image, (fragCoord) / vec2(textureSize(image, 0)));
    imageColor.rgb *= imageColor.a;
    vec4 composeImageColor = texture(composeImage, (fragCoord) / vec2(textureSize(composeImage, 0)));
    composeImageColor.rgb *= composeImageColor.a;
    FragColor = vec4(imageColor.xyz + composeImageColor.xyz, imageColor.w);
    FragColor = vec4(FragColor.xyz, 1.0);
    return;
}
