/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "ge_log.h"
#include "ge_particle_circular_halo_shader.h"
#include "ge_visual_effect_impl.h"

namespace OHOS {
namespace Rosen {
namespace {
    static constexpr char GLOW_HALO_PROG[] = R"(
        uniform half2 iResolution;
        const half2 rotationCenter = half2(0.5);
        const float DOWN_SAMPLE_SCALE = 4.0;

        vec4 fragColor;
        const float PI = 3.14159;
        const float PI2 = 6.28318;

        const half2 RAND2_A = half2(127.1, 1.7);
        const half2 RAND2_B = half2(269.5, 183.3);
        const float RAND2_MUL = 4258.5453123;
        // ****************************** Sub-functions ******************************
        half2 Random2D(half2 st)
        {
            st = half2(dot(st, RAND2_A), dot(st, RAND2_B));
            return -1.0 + 2.0 * fract(sin(st) * RAND2_MUL);
        }

        float Noise2D(half2 st)
        {
            half2 i = floor(st);
            half2 f = fract(st);
            half2 u = smoothstep(0.0, 1.0, f);
            return mix(mix(dot(Random2D(i + half2(0.0, 0.0)), f - half2(0.0, 0.0)),
                           dot(Random2D(i + half2(1.0, 0.0)), f - half2(1.0, 0.0)), u.x),
                       mix(dot(Random2D(i + half2(0.0, 1.0)), f - half2(0.0, 1.0)),
                           dot(Random2D(i + half2(1.0, 1.0)), f - half2(1.0, 1.0)), u.x),
                       u.y);
        }

        float Random1D(float x)
        {
            return fract(sin(x) * RAND2_MUL);
        }

        float Noise1D(float t)
        {
            float f = fract(t);
            float u = smoothstep(0.0, 1.0, f);
            return mix(Random1D(floor(t)), Random1D(floor(t) + 1.0), u);
        }

        float CentralAmbienceHaloBorder(half2 uv, half2 polarCoords, float radius, float animationTime,
            float rotationTimeScale, float noiseScale, float noiseDisplacement)
        {
            polarCoords.x = fract((polarCoords.x - animationTime * 4.0) - (polarCoords.y * 0.8));
            float screenNoise = Noise2D(half2(uv + (animationTime * rotationTimeScale)) * noiseScale);
            float angularRandomVal = Noise1D((polarCoords.x) * 10.0);
            angularRandomVal = mix(angularRandomVal, 0.2, smoothstep(0.5, 0.8, abs(polarCoords.x * 2.0 - 1.0)));
            radius += (screenNoise * noiseDisplacement);
            polarCoords.y -= radius;
            float thickness = mix(0.01, 0.035, angularRandomVal);
            float circleBorder = smoothstep(thickness * radius, 0.0, abs(polarCoords.y));
            circleBorder *= smoothstep(0.1, 1.0, angularRandomVal);
            circleBorder *= screenNoise * 0.5 + 0.5;
            return circleBorder;
        }

        float CentralAmbienceHaloGlow(half2 polarCoords, float animationTime, float radius, float glowRadius,
            float glowExposure)
        {
            polarCoords.x = fract(((polarCoords.x - animationTime * 4.0) - (polarCoords.y * 0.8)));
            float angularRandomVal = Noise1D(polarCoords.x * 10.);
            angularRandomVal = mix(angularRandomVal, 0.2, smoothstep(0.5, 0.8, abs(polarCoords.x * 2.0 - 1.0)));
            polarCoords.y -= radius;
            float circleBorderGlow = smoothstep(glowRadius, 0.0, abs(polarCoords.y)) * glowExposure;
            return circleBorderGlow;
        }

        float BlendScreen(float a, float b)
        {
            return a + b - a * b;
        }

        half4 BlendScreen(half4 a, half4 b)
        {
            return a + b - a * b;
        }

        // ****************************** Main Functions ******************************
        vec4 main(vec2 fragCoord)
        {
            float globalRadius = 0.5 / DOWN_SAMPLE_SCALE;
            float radius = globalRadius * 2.0;
            float innerRadiusEdge = 0.5 * radius;
            float outerRadiusEdge = 1.35 * radius;
            float glowColor = 0.;
            float solidColor = 0.0;

            half2 uv = fragCoord.xy / iResolution.xy;
            uv -= rotationCenter;
            float screenRatio = iResolution.x / iResolution.y;
            half2 centeredUVs = uv * 2.0 ;
            centeredUVs.x *= screenRatio;
            half2 directionVector = centeredUVs;
            float lenDirVec = length(directionVector);
            half2 polarCoords =
                half2((atan(directionVector.y, directionVector.x) + PI) / (2.0 * PI), lenDirVec);

            if (lenDirVec > innerRadiusEdge && lenDirVec < outerRadiusEdge) {
                float ambienceHaloBorder = CentralAmbienceHaloBorder(directionVector, polarCoords, radius * 0.82,
                    globalRadius * 0.1, 1.0, 1.25, 0.05) * 2.5;
                ambienceHaloBorder = clamp(ambienceHaloBorder, 0., 1.);
                solidColor = BlendScreen(solidColor, ambienceHaloBorder);
                ambienceHaloBorder = CentralAmbienceHaloBorder(directionVector, polarCoords, radius * 0.95,
                    globalRadius * 0.25, 1.0, 0.25, 0.05);
                solidColor = BlendScreen(solidColor, ambienceHaloBorder);
                ambienceHaloBorder = CentralAmbienceHaloBorder(directionVector, polarCoords, radius,
                    globalRadius * 0.35, 1.0, 0.25, 0.05);
                solidColor = BlendScreen(solidColor, ambienceHaloBorder);
                float glow = CentralAmbienceHaloGlow(polarCoords, 1.0, radius * 0.83, 0.2 * radius,
                    0.4) * 1.2;
                glowColor = BlendScreen(glowColor, glow);  // Add a glow circle
            }

            fragColor = BlendScreen(half4(half3(solidColor), 0.0), half4(glowColor));
            return fragColor;
        }
    )";

    static constexpr char SINGLE_PARTICLE_HALO_PROG[] = R"(
        uniform half2 iResolution;
        uniform half randomNoise;
        const half2 rotationCenter = half2(0.5);
        const half DOWN_SAMPLE_SCALE = 4.0;

        half4 fragColor;
        // ****************************** Constants ******************************
        const half WIDTH = 0.055;
        const half PI = 3.14159;
        const half PI2 = 6.28318;

        const half2 RAND2_A = half2(127.1, 1.7);
        const half2 RAND2_B = half2(269.5, 183.3);
        const half RAND2_MUL = 2358.5453123;

        // ****************************** Sub-functions ******************************
        half2 Random2D(half2 st)
        {
            st = half2(dot(st, RAND2_A), dot(st, RAND2_B));
            return -1.0 + 2.0 * fract(sin(st) * RAND2_MUL);
        }

        half Noise2D(half2 st)
        {
            half2 i = floor(st);
            half2 f = fract(st);
            half2 u = smoothstep(0.0, 1.0, f);
            return mix(mix(dot(Random2D(i + half2(0.0, 0.0)), f - half2(0.0, 0.0)),
                           dot(Random2D(i + half2(1.0, 0.0)), f - half2(1.0, 0.0)), u.x),
                       mix(dot(Random2D(i + half2(0.0, 1.0)), f - half2(0.0, 1.0)),
                           dot(Random2D(i + half2(1.0, 1.0)), f - half2(1.0, 1.0)), u.x),
                       u.y);
        }

        half Random1D(half x)
        {
            return fract(sin(x) * RAND2_MUL);
        }

        half Noise1D(half t)
        {
            half f = fract(t);
            half u = smoothstep(0.0, 1.0, f);
            return mix(Random1D(floor(t)), Random1D(floor(t) + 1.0), u);
        }

        half2 ShapePerturbation(half2 uv, half noiseVariation, half noiseScale, half noiseStrength)
        {
            half screenNoise = Noise2D(half2(uv.x - noiseVariation, uv.y + noiseVariation) * noiseScale);
            return uv + screenNoise * noiseStrength;
        }

        half SimpleHaloRingShape(half2 uv,  half radius, half noiseVariation, half haloThickness)
        {
            uv = ShapePerturbation(uv, noiseVariation, 1.0, radius * 0.6);
            half2 polarCoords = half2((atan(uv.y, uv.x) + PI) / (2.0 * PI), length(uv));
            polarCoords.y -= radius;
            half angularRandomVal = Noise1D((polarCoords.x) * 10.0);
            // ensure that the radial noise is seamless
            angularRandomVal = mix(angularRandomVal, 0.5, smoothstep(0.9, 1.0, abs(polarCoords.x * 2.0 - 1.0)));
            half thickness = mix(haloThickness, haloThickness * 4.0, angularRandomVal);
            half circleBorder = smoothstep(thickness, thickness * 0.2, abs(polarCoords.y));
            return clamp(circleBorder, 0.0, 1.0);
        }
        // ****************************** Main Functions ******************************
        half4 main(vec2 fragCoord)
        {
          	half globalRadius = 0.5 / DOWN_SAMPLE_SCALE;
            half radius = globalRadius * 2.;
            half innerRadiusEdge = 0.5 * radius;
            half outerRadiusEdge = 1.5 * radius;
            half haloColor = 0.;
            fragColor = half4(0.0);

            half2 uv = fragCoord.xy / iResolution.xy;
            half screenRatio = iResolution.x / iResolution.y;
            uv -= rotationCenter;
            half2 centeredUVs = uv * 2.0;
            centeredUVs.x *= screenRatio;
            half2 directionVector = centeredUVs;
            half lenDir = length(directionVector);
            if (lenDir > innerRadiusEdge && lenDir < outerRadiusEdge) {
                half2 polarCoords =
                half2((atan(directionVector.y, directionVector.x) + PI) / (2.0 * PI), lenDir);

                if (lenDir >= innerRadiusEdge && lenDir <= outerRadiusEdge) {
                    half particleHaloColor = 0.;
                    half haloRing = SimpleHaloRingShape(directionVector, radius, randomNoise, WIDTH * radius);
                    particleHaloColor += haloRing;
                    haloColor = haloColor + particleHaloColor - haloColor * particleHaloColor;
                }
                fragColor = half4(haloColor);
            }
            return fragColor;
        }
    )";

    static constexpr char PARTICLE_HALO_PROG[] = R"(
        uniform half2 iResolution;
        uniform shader singleParticleHalo;
        const half2 rotationCenter = half2(0.5);
        const half DOWN_SAMPLE_SCALE = 4.0;

        vec4 fragColor;
        const int NUMBER_OF_SAMPLES = 11;  // Number of samples used for blur effects
        const float NUMBER_OF_SAMPLES_F = 11.;
        const float PI = 3.14159;
        const float PI2 = 6.28318;

        const half2 RAND2_A = half2(127.1, 1.7);
        const half2 RAND2_B = half2(269.5, 183.3);
        const float RAND2_MUL = 4258.5453123;

        const float COS_ROT1 = 0.993482;
        const float SIN_ROT1 = 0.113991;

        // ****************************** Sub-functions ******************************
        float BlendScreen(float a, float b)
        {
            return a + b - a * b;
        }

        half2 UV2PixelParticle(half2 rotatedDir, float DOWN_SAMPLE_SCALE, float screenRatio, half radius,
            half2 rotationCenter, half2 iRes)
        {
            half lenRotDir = length(rotatedDir);
            half cosBeta = rotatedDir.x / lenRotDir;
            half sinBeta = rotatedDir.y / lenRotDir;
            rotatedDir = half2(cosBeta, sinBeta) * lenRotDir / radius / DOWN_SAMPLE_SCALE;

            rotatedDir /= 2.0;
            rotatedDir.x /= screenRatio;
            rotatedDir += rotationCenter;
            return  rotatedDir * iRes;
        }
		
        // ****************************** Main Functions ******************************
        vec4 main(vec2 fragCoord)
        {
            float radius = 0.5 / DOWN_SAMPLE_SCALE * 2.0;
            float innerRadiusEdge = 0.5 * radius;
            float outerRadiusEdge = 1.5 * radius;
            float haloColor = 0.;
            half4 glowColor = half4(0.0);

            half2 uv = fragCoord.xy / iResolution.xy;
            uv -= rotationCenter;
            float screenRatio = iResolution.x / iResolution.y;
            half2 centeredUVs = uv * 2.0 ;
            centeredUVs.x *= screenRatio;
            half2 directionVector = centeredUVs;
            float lenDirVec = length(directionVector);
            if (lenDirVec >= innerRadiusEdge && lenDirVec <= outerRadiusEdge) {
                float particleHaloColor = 0.;

                half2 rotatedDir = directionVector;
              	half2 samplePixel = UV2PixelParticle(rotatedDir, DOWN_SAMPLE_SCALE, screenRatio, radius,
                    rotationCenter, iResolution.xy);
                half4 samplerColor = singleParticleHalo.eval(samplePixel);
                particleHaloColor += samplerColor.x;
              	
                for (int i = 1; i < NUMBER_OF_SAMPLES; i++) {
                    rotatedDir = mat2(COS_ROT1, SIN_ROT1, -SIN_ROT1, COS_ROT1) * rotatedDir;
                    samplePixel = UV2PixelParticle(rotatedDir, DOWN_SAMPLE_SCALE, screenRatio, radius,
                        rotationCenter, iResolution.xy);
                    samplerColor = singleParticleHalo.eval(samplePixel);
                    particleHaloColor += samplerColor.x;
                }
              
                particleHaloColor /= NUMBER_OF_SAMPLES_F;

                haloColor = BlendScreen(haloColor, particleHaloColor);
            }
            fragColor = half4(haloColor);
            return fragColor;
        }
    )";

    static constexpr char MAIN_SHADER_PROG[] = R"(
        uniform half2 iResolution;
        uniform shader glowHalo;
        uniform shader particleHalo;
        uniform float globalRadius;   // global radius controlling the overall size of halos
        uniform half2 rotationCenter;  // center of the halo rings, normalized coords (0. - 1.)
        const float DOWN_SAMPLE_SCALE = 4.0;

        vec4 fragColor;
        const int NUMBER_OF_SAMPLES = 11;  // Number of samples used for blur effects
        const float NUMBER_OF_SAMPLES_F = 11.;
        const float PI = 3.14159;
        const float PI2 = 6.28318;
        const float C = 0.00001;
        const float S = -0.99999;

        // Color stops for the color bar
        const float STOP_POS0 = 0.00;
        const float STOP_POS1 = 0.25;
        const float STOP_POS2 = 0.75;
        const float STOP_POS3 = 0.96;
        const float STOP_POS4 = 1.00;

        const half3 STOP_COLOR0 = half3(255.0, 184.0, 191.0) / 255.0;  // FFB8BF
        const half3 STOP_COLOR1 = half3(86.0, 146.0, 255.0) / 255.0;   // 5692FF
        const half3 STOP_COLOR2 = half3(137.0, 240.0, 255.0) / 255.0;  // 89F0FF
        const half3 STOP_COLOR3 = half3(255.0, 200.0, 161.0) / 255.0;  // FFC8A1
        const half3 STOP_COLOR4 = half3(253.0, 237.0, 181.0) / 255.0;  // FFEDB5

        const half2 RAND2_A = half2(127.1, 1.7);
        const half2 RAND2_B = half2(269.5, 183.3);
        const float RAND2_MUL = 4258.5453123;

        // ****************************** Sub-functions ******************************
        half3 GetColorFromColorbar(half2 pt, float radius)
        {
            // Map pt.x to the range [0.0, 1.0] for color interpolation
            float t = (pt.x + radius) / (2.0 * radius);
            half3 colorValue =
                step(pt.x, -radius) * STOP_COLOR0 +
                // Return the first color when pt.x is to the left of the halo
                step(STOP_POS0, t) * step(t, STOP_POS1) *
                mix(STOP_COLOR0, STOP_COLOR1,
                    (t - STOP_POS0) / (STOP_POS1 - STOP_POS0)) +  // 0.00 < t <= 0.25
                step(STOP_POS1, t) * step(t, STOP_POS2) *
                mix(STOP_COLOR1, STOP_COLOR2,
                    (t - STOP_POS1) / (STOP_POS2 - STOP_POS1)) +  // 0.25 < t <= 0.75
                step(STOP_POS2, t) * step(t, STOP_POS3) *
                mix(STOP_COLOR2, STOP_COLOR3,
                    (t - STOP_POS2) / (STOP_POS3 - STOP_POS2)) +  // 0.75 < t <= 0.96
                step(STOP_POS3, t) * step(t, STOP_POS4) *
                mix(STOP_COLOR3, STOP_COLOR4,
                    (t - STOP_POS3) / (STOP_POS4 - STOP_POS3)) +  // 0.90 < t <= 1.00
                step(STOP_POS4, t) * STOP_COLOR4;                 // 1.00 < t
            return colorValue;
        }

        half4 BlendScreen(half4 a, half4 b)
        {
            return a + b - a * b;
        }
        
        // ****************************** Main Functions ******************************
        vec4 main(vec2 fragCoord)
        {
            float radius = globalRadius * 2.0;
            float innerRadiusEdge = 0.5 * radius;
            float outerRadiusEdge = 1.5 * radius;
            half4 haloColor = half4(0.0);
            half4 haloColor2 = half4(0.0);
            half4 glowColor = half4(0.0);
          
            vec2 uv = fragCoord / iResolution.xy;
            vec2 ratioMat = vec2(iResolution.x / iResolution.y, 1.0);
          	vec2 centerUV = (uv - rotationCenter) * ratioMat * 2.0;
            float lengthDir = length(centerUV);
			
          	if (lengthDir < outerRadiusEdge && lengthDir > innerRadiusEdge) {
                float rSafe = max(globalRadius, 1.0 / max(iResolution.x, iResolution.y));
                
                const float childRParticle  = 0.5 / DOWN_SAMPLE_SCALE;
                float s1 = childRParticle / rSafe;
                vec2 childUV1 = 0.5 + s1 * (uv - rotationCenter);
                vec2 fc1 = childUV1 * iResolution.xy;
                haloColor = particleHalo.eval(fc1) * 1.2;

                childUV1 = 0.5 + s1 * (uv - rotationCenter) * 0.85;
                haloColor2 = particleHalo.eval(childUV1 * iResolution.xy);

                const float childRGlow = 0.5 / DOWN_SAMPLE_SCALE / 1.2;
                float s2 = childRGlow / rSafe;

                float angle = rSafe * PI2;
                float c = cos(angle);
                float s = sin(angle);
                mat2 rotMat = mat2(c, -s, s, c);
                vec2 rel = (uv - rotationCenter) * ratioMat;
                vec2 relRot  = rotMat * rel / ratioMat;
                
                vec2 childUV2 = 0.5 + s2 * relRot;
                vec2 fc2  = childUV2 * iResolution.xy;
                glowColor.xyz = glowHalo.eval(fc2).xyz;
            
                vec2 centerUVRot = rotMat * centerUV;
                half3 bgColor = GetColorFromColorbar(centerUVRot, radius);

                vec2 centerUVRot2 = mat2(C, -S, S, C) * centerUV;
                half3 bgColor2 = GetColorFromColorbar(centerUVRot2, radius * 1.18);

                glowColor.rgb *= bgColor;
                haloColor.rgb *= bgColor;
                haloColor2.rgb *= bgColor2;
            }
            haloColor = BlendScreen(haloColor, haloColor2);
            fragColor = max(glowColor, haloColor);
            return fragColor;
        }
    )";
}

struct CacheDataType {
    std::shared_ptr<Drawing::Image> particleHaloImg;
    std::shared_ptr<Drawing::Image> glowHaloImg;
    float lastNoise = std::numeric_limits<float>::quiet_NaN();
};

GEParticleCircularHaloShader::GEParticleCircularHaloShader() {}

GEParticleCircularHaloShader::GEParticleCircularHaloShader(Drawing::GEParticleCircularHaloShaderParams& params)
{
    particleCircularHaloParams_ = params;
}

void GEParticleCircularHaloShader::MakeDrawingShader(const Drawing::Rect& rect, float progress)
{
    drShader_ = MakeParticleCircularHaloShader(rect);
}

std::shared_ptr<GEParticleCircularHaloShader> GEParticleCircularHaloShader::CreateParticleCircularHaloShader(
    Drawing::GEParticleCircularHaloShaderParams& params)
{
    std::shared_ptr<GEParticleCircularHaloShader> particleCircularHaloShader =
        std::make_shared<GEParticleCircularHaloShader>(params);
    return particleCircularHaloShader;
}

bool GEParticleCircularHaloShader::IsFloatEqual(float a, float b)
{
    const float eps = 1e-4f;
    return std::fabs(a - b) <= eps * (1.0f + std::max(std::fabs(a), std::fabs(b)));
}

void GEParticleCircularHaloShader::Preprocess(Drawing::Canvas& canvas, const Drawing::Rect& rect)
{
    const float currentNoise = particleCircularHaloParams_.noise_;
    // Helper lambdas to (re)build images
    auto BuildParticleHalo = [this, &canvas, &rect](std::shared_ptr<Drawing::Image>& out) {
        Drawing::ImageInfo imgInfo(rect.GetWidth(), rect.GetHeight(),
                                   Drawing::ColorType::COLORTYPE_RGBA_8888,
                                   Drawing::AlphaType::ALPHATYPE_OPAQUE);
        out = MakeParticleHaloShader(canvas, imgInfo);
    };
    auto BuildGlowHalo = [this, &canvas, &rect](std::shared_ptr<Drawing::Image>& out) {
        Drawing::ImageInfo imgInfo(rect.GetWidth(), rect.GetHeight(),
                                   Drawing::ColorType::COLORTYPE_RGBA_8888,
                                   Drawing::AlphaType::ALPHATYPE_OPAQUE);
        out = MakeGlowHaloShader(canvas, imgInfo);
    };
    CacheDataType* cache = nullptr;
    // Cache exists & noise is unchanged: reuse cache
    if (cacheAnyPtr_ && cacheAnyPtr_->has_value()) {
        cache = std::any_cast<CacheDataType>(cacheAnyPtr_.get());
    }

    // First frame / missing or wrong-type cache: build both and store
    if (!cache) {
        CacheDataType newCache;
        BuildParticleHalo(newCache.particleHaloImg);
        BuildGlowHalo(newCache.glowHaloImg);
        newCache.lastNoise = currentNoise;

        cacheAnyPtr_ = std::make_shared<std::any>(std::move(newCache));
        cache = std::any_cast<CacheDataType>(cacheAnyPtr_.get());
        GE_LOGI("GEParticleCircularHaloShader::Preprocess cacheAnyPtr_ doesn't exist, cache imgs first make.");
    } else {
        // Cache exists but noise is changed: only rebuild particleHalo, glowHalo stays frozen
        if (!IsFloatEqual(cache->lastNoise, currentNoise)) {
            BuildParticleHalo(cache->particleHaloImg);
            cache->lastNoise = currentNoise;
            GE_LOGI("GEParticleCircularHaloShader::Preprocess cacheAnyPtr_ exists, but noise changes,"
                    "update particleHaloImg cache in cacheAnyPtr_");
        }
    }
    // Mirror chched values back to members for the draw passes
    if (cache) {
        particleHaloImg_ = cache->particleHaloImg;
        glowHaloImg_ = cache->glowHaloImg;
    }
}

std::shared_ptr<Drawing::RuntimeShaderBuilder> GEParticleCircularHaloShader::GetGlowHaloBuilder()
{
    thread_local std::shared_ptr<Drawing::RuntimeEffect> glowHaloEffect = nullptr;

    if (glowHaloEffect == nullptr) {
        glowHaloEffect = Drawing::RuntimeEffect::CreateForShader(GLOW_HALO_PROG);
    }
    if (glowHaloEffect == nullptr) {
        GE_LOGE("GEParticleCircularHaloShader::GetGlowHaloBuilder glowHaloEffect is nullptr.");
        return nullptr;
    }
    return std::make_shared<Drawing::RuntimeShaderBuilder>(glowHaloEffect);
}

std::shared_ptr<Drawing::RuntimeShaderBuilder> GEParticleCircularHaloShader::GetSingleParticleHaloBuilder()
{
    thread_local std::shared_ptr<Drawing::RuntimeEffect> singleParticleHaloEffect = nullptr;

    if (singleParticleHaloEffect == nullptr) {
        singleParticleHaloEffect = Drawing::RuntimeEffect::CreateForShader(SINGLE_PARTICLE_HALO_PROG);
    }
    if (singleParticleHaloEffect == nullptr) {
        GE_LOGE("GEParticleCircularHaloShader::GetParticleHaloBuilder singleParticleHaloEffect is "
                "nullptr.");
        return nullptr;
    }
    return std::make_shared<Drawing::RuntimeShaderBuilder>(singleParticleHaloEffect);
}

std::shared_ptr<Drawing::RuntimeShaderBuilder> GEParticleCircularHaloShader::GetParticleHaloBuilder()
{
    thread_local std::shared_ptr<Drawing::RuntimeEffect> particleHaloEffect = nullptr;

    if (particleHaloEffect == nullptr) {
        particleHaloEffect = Drawing::RuntimeEffect::CreateForShader(PARTICLE_HALO_PROG);
    }
    if (particleHaloEffect == nullptr) {
        GE_LOGE("GEParticleCircularHaloShader::GetParticleHaloBuilder particleHaloEffect is "
                "nullptr.");
        return nullptr;
    }
    return std::make_shared<Drawing::RuntimeShaderBuilder>(particleHaloEffect);
}

std::shared_ptr<Drawing::RuntimeShaderBuilder> GEParticleCircularHaloShader::GetParticleCircularHaloBuilder()
{
    thread_local std::shared_ptr<Drawing::RuntimeEffect> glowHaloEffect = nullptr;

    if (glowHaloEffect == nullptr) {
        glowHaloEffect = Drawing::RuntimeEffect::CreateForShader(MAIN_SHADER_PROG);
    }
    if (glowHaloEffect == nullptr) {
        GE_LOGE("GEParticleCircularHaloShader:: GetParticleCircularHaloBuilder glowHaloEffect is nullptr.");
        return nullptr;
    }
    return std::make_shared<Drawing::RuntimeShaderBuilder>(glowHaloEffect);
}

std::shared_ptr<Drawing::Image> GEParticleCircularHaloShader::MakeGlowHaloShader(Drawing::Canvas& canvas,
                                                                                 const Drawing::ImageInfo& imageInfo)
{
    float width = imageInfo.GetWidth();
    float height = imageInfo.GetHeight();
    glowHaloBuilder_ = GetGlowHaloBuilder();
    if (glowHaloBuilder_ == nullptr) {
        GE_LOGE("GEParticleCircularHaloShader MakeGlowHaloShader preCalculatedBuilder_ is nullptr.");
        return nullptr;
    }
    glowHaloBuilder_->SetUniform("iResolution", width, height);
    auto glowHaloShader = glowHaloBuilder_->MakeImage(canvas.GetGPUContext().get(), nullptr, imageInfo, false);
    if (glowHaloShader == nullptr) {
        GE_LOGE("GEParticleCircularHaloShader glowHaloShader is nullptr.");
        return nullptr;
    }
    return glowHaloShader;
}

std::shared_ptr<Drawing::Image> GEParticleCircularHaloShader::MakeSingleParticleHaloShader(
    Drawing::Canvas& canvas, const Drawing::ImageInfo& imageInfo)
{
    float width = imageInfo.GetWidth();
    float height = imageInfo.GetHeight();

    singleParticleHaloBuilder_ = GetSingleParticleHaloBuilder();
    if (singleParticleHaloBuilder_ == nullptr) {
        GE_LOGE("GEParticleCircularHaloShader::MakeSingleParticleHaloShader singleParticleHaloBuilder_ is nullptr.");
        return nullptr;
    }
    singleParticleHaloBuilder_->SetUniform("iResolution", width, height);
    singleParticleHaloBuilder_->SetUniform("randomNoise", particleCircularHaloParams_.noise_);
    auto singleParticleHaloShader =
        singleParticleHaloBuilder_->MakeImage(canvas.GetGPUContext().get(), nullptr, imageInfo, false);
    if (singleParticleHaloShader == nullptr) {
        GE_LOGE("GEParticleCircularHaloShader::MakeSingleParticleHaloShader is nullptr.");
        return nullptr;
    }
    return singleParticleHaloShader;
}

std::shared_ptr<Drawing::Image> GEParticleCircularHaloShader::MakeParticleHaloShader(
    Drawing::Canvas& canvas, const Drawing::ImageInfo& imageInfo)
{
    float width = imageInfo.GetWidth();
    float height = imageInfo.GetHeight();
    auto singleParticleHaloImg = MakeSingleParticleHaloShader(canvas, imageInfo);
    if (singleParticleHaloImg == nullptr) {
        GE_LOGE("GEParticleCircularHaloShader MakeParticleCircularHaloShader singleParticleHaloImg is nullptr.");
        return nullptr;
    }
    particleHaloBuilder_ = GetParticleHaloBuilder();
    if (particleHaloBuilder_ == nullptr) {
        GE_LOGE("GEParticleCircularHaloShader MakeParticleHaloShader particleHaloBuilder_ is nullptr.");
        return nullptr;
    }
    Drawing::Matrix matrix;
    auto singleParticleHaloShader = Drawing::ShaderEffect::CreateImageShader(
        *singleParticleHaloImg, Drawing::TileMode::CLAMP, Drawing::TileMode::CLAMP,
        Drawing::SamplingOptions(Drawing::FilterMode::LINEAR), matrix);
    if (singleParticleHaloShader == nullptr) {
        GE_LOGE("GEParticleCircularHaloShader MakeParticleHaloShader singleParticleHaloShader is nullptr.");
        return nullptr;
    }
    particleHaloBuilder_->SetUniform("iResolution", width, height);
    particleHaloBuilder_->SetChild("singleParticleHalo", singleParticleHaloShader);
    auto particleHaloShader = particleHaloBuilder_->MakeImage(canvas.GetGPUContext().get(), nullptr, imageInfo, false);
    if (particleHaloShader == nullptr) {
        GE_LOGE("GEParticleCircularHaloShader MakeParticleHaloShader is nullptr.");
        return nullptr;
    }
    return particleHaloShader;
}

std::shared_ptr<Drawing::ShaderEffect> GEParticleCircularHaloShader::MakeParticleCircularHaloShader(
    const Drawing::Rect& rect)
{
    auto width = rect.GetWidth();
    auto height = rect.GetHeight();
    if (glowHaloImg_ == nullptr || particleHaloImg_ == nullptr) {
        GE_LOGE("GEParticleCircularHaloShader::MakeParticleCircularHaloShader img_s are nullptr.");
        return nullptr;
    }
    Drawing::Matrix matrix;
    auto glowHaloShader =
        Drawing::ShaderEffect::CreateImageShader(*glowHaloImg_, Drawing::TileMode::CLAMP, Drawing::TileMode::CLAMP,
                                                 Drawing::SamplingOptions(Drawing::FilterMode::LINEAR), matrix);
    if (glowHaloShader == nullptr) {
        GE_LOGE("GEParticleCircularHaloShader::MakeParticleCircularHaloShader glowHaloShader is nullptr.");
        return nullptr;
    }
    auto particleHaloShader =
        Drawing::ShaderEffect::CreateImageShader(*particleHaloImg_, Drawing::TileMode::CLAMP, Drawing::TileMode::CLAMP,
                                                 Drawing::SamplingOptions(Drawing::FilterMode::LINEAR), matrix);
    if (particleHaloShader == nullptr) {
        GE_LOGE("GEParticleCircularHaloShader::MakeParticleCircularHaloShader particleHaloShader is nullptr.");
        return nullptr;
    }
    builder_ = GetParticleCircularHaloBuilder();
    if (builder_ == nullptr) {
        GE_LOGE("GEParticleCircularHaloShader::MakeParticleCircularHaloShader builder_ is nullptr.");
        return nullptr;
    }
    builder_->SetUniform("iResolution", width, height);
    builder_->SetChild("glowHalo", glowHaloShader);
    builder_->SetChild("particleHalo", particleHaloShader);
    builder_->SetUniform("globalRadius", particleCircularHaloParams_.radius_);
    builder_->SetUniform("rotationCenter", particleCircularHaloParams_.center_.first,
                         particleCircularHaloParams_.center_.second);
    auto particleCircularHaloShader = builder_->MakeShader(nullptr, false);
    if (particleCircularHaloShader == nullptr) {
        GE_LOGE("GEParticleCircularHaloShader::MakeParticleCircularHaloShader particleCircularHaloShader is nullptr.");
        return nullptr;
    }
    return particleCircularHaloShader;
}

}  // namespace Rosen
}  // namespace OHOS