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
#include <algorithm>
#include "ge_log.h"
#include "ge_wavy_ripple_light_shader.h"

namespace OHOS {
namespace Rosen {

GEWavyRippleLightShader::GEWavyRippleLightShader() {}

GEWavyRippleLightShader::GEWavyRippleLightShader(Drawing::GEWavyRippleLightShaderParams& param)
{
    wavyRippleLightParams_ = param;
}

void GEWavyRippleLightShader::MakeDrawingShader(const Drawing::Rect& rect, float progress)
{
    drShader_ = MakeWavyRippleLightShader(rect);
}

std::shared_ptr<Drawing::RuntimeShaderBuilder> GEWavyRippleLightShader::GetWavyRippleLightBuilder()
{
    thread_local std::shared_ptr<Drawing::RuntimeEffect> wavyRippleLightShaderEffect_ = nullptr;

    if (wavyRippleLightShaderEffect_ == nullptr) {
        static constexpr char prog[] = R"(
            uniform vec2 iResolution;
            uniform vec2 center;
            uniform float radius;
            uniform float thickness;

            const float noiseScale = 0.3;
            const float waveScale  = 0.5;

            const float freqX = 4.0;
            const float freqY = 6.0;
            const float freqDiag = 8.0;

            float shapeSDF(vec2 p, float radius)
            {
                float dist = length(p);

                float noise = 0.0;
                noise += sin(p.x * freqX) * 0.25;
                noise += sin(p.y * freqY) * 0.35;
                noise += sin((p.x + p.y) * freqDiag) * 0.1;

                float distortedDist = dist + noise * noiseScale;
                float attenuation = waveScale / (1.0 + distortedDist * 5.0);
                float wave = sin(distortedDist * 30.0) * attenuation;

                return distortedDist - radius + wave * 0.05;
            }

            vec4 main(vec2 fragCoord)
            {
                vec2 uv = fragCoord.xy / iResolution.xy;
                float aspect = iResolution.x / iResolution.y;
                vec3 color = vec3(0.0);

                vec2 p = uv * 2.0 - 1.0;
                p.x *= aspect;
                vec2 c = center * 2.0 - 1.0;
                c.x *= aspect;

                vec2 delta = p - c;

                float dist0 = length(c - vec2(-aspect, -1.0));
                float dist1 = length(c - vec2(-aspect,  1.0));
                float dist2 = length(c - vec2(aspect, -1.0));
                float dist3 = length(c - vec2(aspect,  1.0));
                float maxRadius = thickness + max(max(dist0, dist1), max(dist2, dist3));

                float currentRadius = maxRadius * radius;
                float d = abs(shapeSDF(delta, currentRadius));
                float mask = smoothstep(thickness, 0.0, d);

                vec3 baseColor = vec3(0.8, 0.5, 1.0);
                color = baseColor * mask;

                return vec4(color, mask);
            }
        )";
        wavyRippleLightShaderEffect_ = Drawing::RuntimeEffect::CreateForShader(prog);
    }

    if (wavyRippleLightShaderEffect_ == nullptr) {
        GE_LOGE("GEWavyRippleLightShader::GetWavyRippleLightBuilder wavyRippleLightShaderEffect_ is nullptr.");
        return nullptr;
    }
    return std::make_shared<Drawing::RuntimeShaderBuilder>(wavyRippleLightShaderEffect_);
}

std::shared_ptr<Drawing::ShaderEffect> GEWavyRippleLightShader::MakeWavyRippleLightShader(const Drawing::Rect& rect)
{
    auto width = rect.GetWidth();
    auto height = rect.GetHeight();
    builder_ = GetWavyRippleLightBuilder();
    if (builder_ == nullptr) {
        GE_LOGE("GEWavyRippleLightShader::MakeWavyRippleLightShader builder_ is nullptr.");
        return nullptr;
    }
    builder_->SetUniform("iResolution", width, height);
    builder_->SetUniform("center", wavyRippleLightParams_.center_.first,
        wavyRippleLightParams_.center_.second);
    builder_->SetUniform("radius", std::max(0.0f, wavyRippleLightParams_.radius_));
    builder_->SetUniform("thickness", std::max(0.01f, wavyRippleLightParams_.thickness_)); // 0.01f: minimum thickness
    auto wavyRippleLightShader = builder_->MakeShader(nullptr, false);

    if (wavyRippleLightShader == nullptr) {
        GE_LOGE("GEWavyRippleLightShader::MakeWavyRippleLightShader wavyRippleLightShader is nullptr.");
        return nullptr;
    }
    return wavyRippleLightShader;
}

} // namespace Rosen
} // namespace OHOS