#version 450 core
out vec4 FragColor;
uniform sampler2D image;
uniform sampler2D mask;
uniform vec2 iResolution;
uniform float opacity;
uniform vec2 redOffset;
uniform vec2 greenOffset;
uniform vec2 blueOffset;
void main() {
    vec2 fragCoord = gl_FragCoord.xy;
    vec2 fragUV = fragCoord / iResolution;
    vec4 maskColor = texture(mask, (fragCoord) / iResolution);
    vec2 sdf = (maskColor.xy - 0.5) * 2.0;
    float alpha = maskColor.w * opacity;
    vec3 dispersedColor = vec3(0.0);
    vec2 offset = redOffset * sdf;
    dispersedColor.x = texture(image, fragUV + offset).x;
    offset = greenOffset * sdf;
    dispersedColor.y = texture(image, fragUV + offset).y;
    offset = blueOffset * sdf;
    dispersedColor.z = texture(image, fragUV + offset).z;
    vec4 imageColor = texture(image, (fragCoord) / iResolution);
    vec3 finalColor = mix(imageColor.xyz, dispersedColor, vec3(alpha));
    FragColor = vec4(finalColor, imageColor.w);
    return;
}
