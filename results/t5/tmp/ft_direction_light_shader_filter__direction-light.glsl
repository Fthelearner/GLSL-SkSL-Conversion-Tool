### Compilation failed:

error: 8: variables of type 'shader' may not be uniform
uniform shader image;
^^^^^^^^^^^^^^^^^^^^
error: 9: variables of type 'shader' may not be uniform
uniform shader mask;
^^^^^^^^^^^^^^^^^^^
error: 59: type 'shader' has no method named 'eval'
    float3 albedo = image.eval(pos.xy).xyz;
                    ^^^^^^^^^^
error: 60: unknown identifier 'albedo'
    float3 diffuseColor = mix(albedo, float3(0.0, 0.0, 0.0), float3(0.5, 0.5, 0.5));
                              ^^^^^^
error: 61: unknown identifier 'albedo'
    float3 specularColor = mix(float3(0.032, 0.032, 0.032), albedo, float3(0.5, 0.5, 0.5));
                                                            ^^^^^^
error: 62: unknown identifier 'specularColor'
    float3 fresnel = FresnelSchlick_f3f3f(specularColor, cosVToH);
                                          ^^^^^^^^^^^^^
error: 63: unknown identifier 'fresnel'
    float3 reflectedColor = distribution * visibility * fresnel * cosNToL * lightColor.xyz;
                                                        ^^^^^^^
error: 64: unknown identifier 'diffuseColor'
    float3 refractedColor = diffuseColor * cosNToL * lightColor.xyz;
                            ^^^^^^^^^^^^
error: 65: unknown identifier 'reflectedColor'
    return float4((reflectedColor + refractedColor) * lightIntensity, 1.0);
                   ^^^^^^^^^^^^^^
error: 65: unknown identifier 'refractedColor'
    return float4((reflectedColor + refractedColor) * lightIntensity, 1.0);
                                    ^^^^^^^^^^^^^^
error: 71: type 'shader' has no method named 'eval'
    float3 tempNormalValue = mask.eval(fragCoord).xyz;
                             ^^^^^^^^^
error: 72: unknown identifier 'tempNormalValue'
    float3 normal = normalize(tempNormalValue * 2.0 - 1.0);
                              ^^^^^^^^^^^^^^^
error: 73: call to 'all' expected 1 argument, but found 0
    if (all(/* unhandled aggregate:  */)) {
        ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
error: 74: unknown identifier 'normal'
        normal = float3(0.0, 0.0, 1.0);
        ^^^^^^
error: 76: unknown identifier 'normal'
    float3 displacementNormal = normalize(float3(0.787401557, 0.787401557, 0.5) * normal);
                                                                                  ^^^^^^
error: 77: unknown identifier 'normal'
    float3 shadingNormal = normalize(float3(0.787401557, 0.787401557, 10.0) * normal);
                                                                              ^^^^^^
error: 78: unknown identifier 'displacementNormal'
    FragColor = scatter_f4f3f3f3f(pos, displacementNormal, shadingNormal, 0.2);
                                       ^^^^^^^^^^^^^^^^^^
error: 78: unknown identifier 'shadingNormal'
    FragColor = scatter_f4f3f3f3f(pos, displacementNormal, shadingNormal, 0.2);
                                                           ^^^^^^^^^^^^^
18 errors
