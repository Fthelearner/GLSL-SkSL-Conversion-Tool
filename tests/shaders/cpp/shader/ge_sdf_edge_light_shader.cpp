/*
 * Copyright (c) 2026 Huawei Device Co., Ltd.
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

#include "ge_sdf_edge_light_shader.h"

#include <unordered_map>

#include "ge_log.h"

namespace OHOS::Rosen {
namespace {

constexpr char SHADER[] = R"(
    uniform shader sdfShader;
    uniform shader lightMaskShader;

    uniform vec3 lightColor;
    uniform float bloomIntensityCutoff;
    uniform float maxIntensity;
    uniform float maxBloomIntensity;
    uniform float bloomFalloffPow;

    uniform float minBorderWidth;
    uniform float borderWidthDelta;

    uniform float innerBorderBloomWidth;
    uniform float outerBorderBloomWidth;

    uniform float invInnerBorderBloomWidth;
    uniform float invOuterBorderBloomWidth;
    uniform float enableBloom;

    float BloomMultiplierFromDist(float d)
    {
        float dNorm = (d > 0.0) ? (d * invOuterBorderBloomWidth) : (-d * invInnerBorderBloomWidth);
        float falloff = max((1.0 - dNorm) / pow(1.0 + dNorm, bloomFalloffPow), 0.0);
        return maxBloomIntensity * falloff;
    }

    float EdgeLightFakeBloom(float intensity, float d)
    {
        float bloomBorder = 1.0 - step(outerBorderBloomWidth, d);
        bloomBorder *= step(-innerBorderBloomWidth, d);

        float edgeThickness = smoothstep(0.0, 1.0, intensity) * borderWidthDelta + minBorderWidth;
        float thinBorder = 1.0 - smoothstep(0.0, edgeThickness, abs(d));

        float bloomGate = smoothstep(bloomIntensityCutoff, 1.0, intensity);
        float bloomPart = 0.0;
        if (enableBloom > 0.0 && bloomGate > 0.0 && bloomBorder > 0.0) {
            bloomPart = bloomGate * bloomBorder * BloomMultiplierFromDist(d);
        }

        float b = intensity * maxIntensity * (thinBorder + bloomPart);
        return b;
    }

    half4 main(vec2 fragCoord)
    {
        float lightMaskValue = lightMaskShader.eval(fragCoord).r;
        vec4 sdfSample = sdfShader.eval(fragCoord);
        float alpha = EdgeLightFakeBloom(lightMaskValue, sdfSample.a);
        return half4(lightColor * alpha, alpha);
    }
)";
} // namespace

GESDFEdgeLightShader::GESDFEdgeLightShader(const Drawing::GESDFEdgeLightEffectParams& params)
    : params_(params)
{}

void GESDFEdgeLightShader::MakeDrawingShader(Drawing::Canvas& canvas, const Drawing::Rect& rect, float progress)
{
    drShader_ = nullptr;

    if (!params_.sdfShape) {
        LOGE("GESDFEdgeLightShader::MakeDrawingShader sdfShape is null");
        return;
    }

    if (!params_.lightMask) {
        LOGE("GESDFEdgeLightShader::MakeDrawingShader lightMask is null");
        return;
    }

    auto builder = GetEffectShaderBuilder(canvas, rect);
    if (builder == nullptr) {
        LOGE("GESDFEdgeLightShader::MakeDrawingShader builder is nullptr");
        return;
    }

    auto sdfEdgeLightShader = builder->MakeShader(nullptr, false);
    if (sdfEdgeLightShader == nullptr) {
        LOGE("GESDFEdgeLightShader::MakeDrawingShader sdfEdgeLightShader is nullptr");
    }
    drShader_ = sdfEdgeLightShader;
}

std::shared_ptr<Drawing::RuntimeShaderBuilder> GESDFEdgeLightShader::GetEffectShaderBuilder(
    Drawing::Canvas& canvas, const Drawing::Rect& rect)
{
    thread_local std::shared_ptr<Drawing::RuntimeEffect> effectShader_ = nullptr;

    if (effectShader_ == nullptr) {
        effectShader_ = Drawing::RuntimeEffect::CreateForShader(SHADER);
    }

    if (effectShader_ == nullptr) {
        LOGE("GESDFEdgeLightShader::GetEffectShaderBuilder effectShader_ is nullptr");
        return nullptr;
    }

    auto width = canvasInfo_.geoWidth;
    auto height = canvasInfo_.geoHeight;

    auto sdfShader = params_.sdfShape->GenerateDrawingShader(canvas, width, height);
    if (sdfShader == nullptr) {
        LOGE("GESDFEdgeLightShader::GetEffectShaderBuilder sdfShader is nullptr");
        return nullptr;
    }
    auto lightMaskShader = params_.lightMask->GenerateDrawingShader(width, height);
    if (lightMaskShader == nullptr) {
        LOGE("GESDFEdgeLightShader::GetEffectShaderBuilder lightMaskShader is nullptr");
        return nullptr;
    }

    auto builder = std::make_shared<Drawing::RuntimeShaderBuilder>(effectShader_);
    builder->SetChild("sdfShader", sdfShader);
    builder->SetChild("lightMaskShader", lightMaskShader);
    builder->SetUniform("lightColor", params_.color[0], params_.color[1], params_.color[2]); // 0 is R, 1 is G, 2 is B
    builder->SetUniform("bloomIntensityCutoff", params_.bloomIntensityCutoff);
    builder->SetUniform("maxIntensity", params_.maxIntensity);
    builder->SetUniform("maxBloomIntensity", params_.maxBloomIntensity);
    builder->SetUniform("bloomFalloffPow", params_.bloomFalloffPow);

    builder->SetUniform("minBorderWidth", params_.minBorderWidth);
    builder->SetUniform("borderWidthDelta", params_.maxBorderWidth - params_.minBorderWidth);
    builder->SetUniform("innerBorderBloomWidth",
        std::max(params_.innerBorderBloomWidth, std::numeric_limits<float>::epsilon()));
    builder->SetUniform("outerBorderBloomWidth",
        std::max(params_.outerBorderBloomWidth, std::numeric_limits<float>::epsilon()));
    builder->SetUniform("invInnerBorderBloomWidth",
        1.0f / std::max(params_.innerBorderBloomWidth, std::numeric_limits<float>::epsilon()));
    builder->SetUniform("invOuterBorderBloomWidth",
        1.0f / std::max(params_.outerBorderBloomWidth, std::numeric_limits<float>::epsilon()));
    const float enableBloom = (params_.maxBloomIntensity > 0.0f &&
        params_.innerBorderBloomWidth > std::numeric_limits<float>::epsilon() &&
        params_.outerBorderBloomWidth > std::numeric_limits<float>::epsilon()) ? 1.0f : 0.0f;
    builder->SetUniform("enableBloom", enableBloom);

    return builder;
}
} // namespace OHOS::Rosen
