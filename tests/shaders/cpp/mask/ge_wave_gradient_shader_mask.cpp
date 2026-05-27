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
#include <chrono>

#include "ge_log.h"
#include "ge_wave_gradient_shader_mask.h"

namespace OHOS {
namespace Rosen {
namespace Drawing {

GEWaveGradientShaderMask::GEWaveGradientShaderMask(GEWaveGradientShaderMaskParams param) : param_(param) {}

std::shared_ptr<ShaderEffect> GEWaveGradientShaderMask::GenerateDrawingShader(float width, float height) const
{
    std::shared_ptr<Drawing::RuntimeShaderBuilder> builder = nullptr;
    builder = GetWaveGradientShaderMaskBuilder();
    if (!builder) {
        LOGE("GEWaveGradientShaderMask::GenerateDrawingShaderHas builder error");
        return nullptr;
    }
    builder->SetUniform("iResolution", width, height);
    builder->SetUniform("waveCenter", param_.center_.first, param_.center_.second);
    builder->SetUniform("waveWidth", param_.width_);
    builder->SetUniform("turbulenceStrength", param_.turbulenceStrength_);
    builder->SetUniform("blurRadius", param_.blurRadius_);
    builder->SetUniform("propagationRadius", param_.propagationRadius_);
    auto waveMaskEffectShader = builder->MakeShader(nullptr, false);
    if (!waveMaskEffectShader) {
        LOGE("GEWaveGradientShaderMask::GenerateDrawingShader effect build failed");
    }
    return waveMaskEffectShader;
}

std::shared_ptr<Drawing::RuntimeShaderBuilder> GEWaveGradientShaderMask::GetWaveGradientShaderMaskBuilder() const
{
    thread_local std::shared_ptr<Drawing::RuntimeShaderBuilder> waveShaderMaskBuilder = nullptr;
    if (waveShaderMaskBuilder) {
        return waveShaderMaskBuilder;
    }

    static constexpr char prog[] = R"(
        uniform half2 iResolution;
        uniform half2 waveCenter;
        uniform half waveWidth;
        uniform half turbulenceStrength;
        uniform half blurRadius;
        uniform half propagationRadius;

        vec2 random2(vec2 st)
        {
            // Magic constants for dot products: chosen to avoid grid artifacts and ensure good distribution
            return fract(sin(vec2(dot(st, vec2(127.1, 311.7)),
                dot(st, vec2(269.5, 183.3)))) * 43758.5453) * 2.0 - 1.0;
        }

        float noise(vec2 st)
        {
            vec2 i = floor(st);
            vec2 f = fract(st);
            vec2 u = f * f * (3.0 - 2.0 * f);
            return mix(
                mix(dot(random2(i), f),
                    dot(random2(i + vec2(1.0, 0.0)), f - vec2(1.0, 0.0)), u.x),
                mix(dot(random2(i + vec2(0.0, 1.0)), f - vec2(0.0, 1.0)),
                    dot(random2(i + vec2(1.0, 1.0)), f - vec2(1.0, 1.0)), u.x),
                u.y);
        }

        float fbm(vec2 st)
        {
            float value = 0.0;
            float amp = 0.5;
            value += amp * noise(st);
            st *= 2.0;
            amp = 0.25; // half of origin value
            value += amp * noise(st);
            return value;
        }

        half4 main(vec2 fragCoord)
        {
            vec2 uv = fragCoord.xy / iResolution.xy;
            
            float aspect = iResolution.x / iResolution.y;
            vec2 center = vec2(waveCenter.x * aspect, waveCenter.y);
            float currentRadius = propagationRadius;

            float turbulenceTime = currentRadius * 5.0 + sin(currentRadius * 6.28) * 2.0;
            float n1 = fbm(uv * 3.0 + vec2(turbulenceTime, 0.0));
            float n2 = fbm(uv * 3.0 + vec2(0.0, turbulenceTime));
            vec2 turbulenceOffset = vec2(n1, n2) * turbulenceStrength;

            vec2 uvWarped = uv + turbulenceOffset;
            vec2 uvCorrectedWarped = vec2(uvWarped.x * aspect, uvWarped.y);
            vec2 dirWarped = uvCorrectedWarped - center;
            float distWarped = length(dirWarped);

            float mask;
            if (waveWidth < 0.0001) {
                float outer = currentRadius + blurRadius;
                mask = 1.0 - smoothstep(currentRadius - blurRadius, outer, distWarped);
            } else {
                float inner = currentRadius - waveWidth - blurRadius;
                float outer = currentRadius + waveWidth + blurRadius;
                mask = smoothstep(inner, inner + blurRadius, distWarped)
                    - smoothstep(outer - blurRadius, outer, distWarped);
            }
            return half4(mask);
        }
    )";

    auto waveShaderMaskEffect = Drawing::RuntimeEffect::CreateForShader(prog);
    if (!waveShaderMaskEffect) {
        LOGE("GEWaveGradientShaderMask::GetWaveShaderMaskBuilder effect error");
        return nullptr;
    }

    waveShaderMaskBuilder = std::make_shared<Drawing::RuntimeShaderBuilder>(waveShaderMaskEffect);
    return waveShaderMaskBuilder;
}

std::shared_ptr<ShaderEffect> GEWaveGradientShaderMask::GenerateDrawingShaderHasNormal(float width, float height) const
{
    std::shared_ptr<Drawing::RuntimeShaderBuilder> builder = nullptr;
    builder = GetWaveGradientShaderNormalMaskBuilder();
    if (!builder) {
        LOGE("GEWaveGradientShaderMask::GenerateDrawingShaderHasNormal builder error");
        return nullptr;
    }
    builder->SetUniform("iResolution", width, height);
    builder->SetUniform("waveCenter", param_.center_.first, param_.center_.second);
    builder->SetUniform("waveWidth", param_.width_);
    builder->SetUniform("turbulenceStrength", param_.turbulenceStrength_);
    builder->SetUniform("blurRadius", param_.blurRadius_);
    builder->SetUniform("propagationRadius", param_.propagationRadius_);
    auto waveMaskEffectShader = builder->MakeShader(nullptr, false);
    if (!waveMaskEffectShader) {
        LOGE("GEWaveGradientShaderMask::GenerateDrawingShaderHasNormal effect error");
    }
    return waveMaskEffectShader;
}

std::shared_ptr<Drawing::RuntimeShaderBuilder> GEWaveGradientShaderMask::GetWaveGradientShaderNormalMaskBuilder() const
{
    thread_local std::shared_ptr<Drawing::RuntimeShaderBuilder> waveShaderMaskNormalBuilder = nullptr;
    if (waveShaderMaskNormalBuilder) {
        return waveShaderMaskNormalBuilder;
    }

    static constexpr char prog[] = R"(
        uniform half2 iResolution;
        uniform half2 waveCenter;
        uniform half waveWidth;
        uniform half turbulenceStrength;
        uniform half blurRadius;
        uniform half propagationRadius;

        vec2 random2(vec2 st)
        {
            // Magic constants for dot products: chosen to avoid grid artifacts and ensure good distribution
            return fract(sin(vec2(dot(st, vec2(127.1, 311.7)),
                dot(st, vec2(269.5, 183.3)))) * 43758.5453) * 2.0 - 1.0;
        }

        float noise(vec2 st)
        {
            vec2 i = floor(st);
            vec2 f = fract(st);
            vec2 u = f * f * (3.0 - 2.0 * f);
            return mix(
                mix(dot(random2(i), f),
                    dot(random2(i + vec2(1.0, 0.0)), f - vec2(1.0, 0.0)), u.x),
                mix(dot(random2(i + vec2(0.0, 1.0)), f - vec2(0.0, 1.0)),
                    dot(random2(i + vec2(1.0, 1.0)), f - vec2(1.0, 1.0)), u.x),
                u.y);
        }

        float fbm(vec2 st)
        {
            float value = 0.0;
            float amp = 0.5;
            value += amp * noise(st);
            st *= 2.0;
            amp = 0.25; // half of origin value
            value += amp * noise(st);
            return value;
        }

        half4 main(vec2 fragCoord)
        {
            vec2 uv = fragCoord.xy / iResolution.xy;
            
            float aspect = iResolution.x / iResolution.y;
            vec2 center = vec2(waveCenter.x * aspect, waveCenter.y);
            float currentRadius = propagationRadius;

            float turbulenceTime = currentRadius * 5.0 + sin(currentRadius * 6.28) * 2.0;
            float n1 = fbm(uv * 3.0 + vec2(turbulenceTime, 0.0));
            float n2 = fbm(uv * 3.0 + vec2(0.0, turbulenceTime));
            vec2 turbulenceOffset = vec2(n1, n2) * turbulenceStrength;

            vec2 uvWarped = uv + turbulenceOffset;
            vec2 uvCorrectedWarped = vec2(uvWarped.x * aspect, uvWarped.y);
            vec2 dirWarped = uvCorrectedWarped - center;
            float distWarped = length(dirWarped);

            float mask;
            if (waveWidth < 0.0001) {
                float outer = currentRadius + blurRadius;
                mask = 1.0 - smoothstep(currentRadius - blurRadius, outer, distWarped);
            } else {
                float inner = currentRadius - waveWidth - blurRadius;
                float outer = currentRadius + waveWidth + blurRadius;
                mask = smoothstep(inner, inner + blurRadius, distWarped)
                    - smoothstep(outer - blurRadius, outer, distWarped);
            }
            vec2 direction = vec2(0.0);
            if (mask > 0.001) {
                vec2 radialDir = normalize(dirWarped);
                vec2 turbulentDir = normalize(turbulenceOffset + 0.0001);
                direction = radialDir * 0.4 + turbulentDir * turbulenceStrength * 3.0;
                direction *= mask;
            }
            direction = clamp(direction, -1.0, 1.0);
            vec2 normalizedDirection = direction * 0.5 + 0.5;

            return half4(normalizedDirection, 1.0, mask);
        }
    )";

    auto waveShaderMaskNormalEffect = Drawing::RuntimeEffect::CreateForShader(prog);
    if (!waveShaderMaskNormalEffect) {
        LOGE("GEWaveGradientShaderMask::GetWaveShaderNormalMaskBuilder effect error");
        return nullptr;
    }

    waveShaderMaskNormalBuilder = std::make_shared<Drawing::RuntimeShaderBuilder>(waveShaderMaskNormalEffect);
    return waveShaderMaskNormalBuilder;
}
} // namespace Drawing
} // namespace Rosen
} // namespace OHOS