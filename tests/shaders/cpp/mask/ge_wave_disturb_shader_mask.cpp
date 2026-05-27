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

#include "ge_wave_disturb_shader_mask.h"
#include "ge_log.h"
#include "ge_trace.h"

namespace OHOS {
namespace Rosen {
namespace Drawing {
constexpr size_t NUM_0 = 0;
constexpr size_t NUM_1 = 1;
constexpr size_t NUM_2 = 2;
static constexpr char WAVE_DISTURBANCE_PROG[] = R"(
    // ----- wave parameters -----
    uniform vec2 clickPos;
    uniform float processTime;
    uniform float waveLength;
    uniform float reflectRatio;
    uniform float waveWidth;
    uniform float waveHeight;
    uniform float waveDown;

    const float PI = 3.14159265358979;
    float FastSin(float x)
    {
        x = x < PI / 2. ? x : PI - x;
        return x * (1. - (1. / 6.) * x * x); // Taylor for sin(x)
    }

    float FastCos(float x)
    {
        if (x < PI / 2.) {
            float x2 = x * x;
            return 1. - 0.5 * x2 + (1. / 24.) * x2 * x2;
        }
        else {
            x = PI - x;
            float x2 = x * x;
            return - (1. - 0.5 * x2 + (1. / 24.) * x2 * x2);
        }
    }
    
    // distortion for wave
    vec4 main(vec2 fragCoord)
    {
        float radius = processTime * waveLength;
        vec2 centerVec = fragCoord.xy - clickPos;
        float d = length(centerVec);
        float rl = d - radius; // ring length
        if (clickPos.x >= 0 && abs(rl) < 2.0 * waveWidth - 1e-5) {
            float rate = processTime > waveDown ? (1.0 - processTime) / (1.0 - waveDown + 1e-5) : 1.;
            float height = waveHeight * rate;
            float uniformRl = abs(rl) * PI / 2.0 / waveWidth;
            float tantheta1 = height * FastSin(uniformRl) * PI / waveWidth + 1e-6;
            float tantheta0 = 1.0 / tantheta1;
            float sintheta2 = tantheta1 / sqrt(tantheta1 * tantheta1 + 1.0) / reflectRatio;
            float tantheta2 = sintheta2 / sqrt(1. - sintheta2 * sintheta2);
            float slop = (tantheta0 + tantheta2) / (1. - tantheta0 * tantheta2);
            float relativeHeight = (FastCos(uniformRl) + 1.0);
            float detx = (-height * relativeHeight) / slop;
            detx = rl > 0. ? detx : -detx;
            // move pixel
            vec2 normal = d > 1e-4 ? normalize(centerVec) : vec2(0., 1.);
            return vec4(normal * detx, relativeHeight / 2. * rate, 1.);
        }
        return vec4(0., 0., 0., 1.);
    }
)";

std::shared_ptr<ShaderEffect> GEWaveDisturbanceShaderMask::GenerateDrawingShader(float width,
    float height) const
{
    GE_TRACE_NAME_FMT("GEWaveDisturbanceShaderMask::GenerateDrawingShader, Width: %g, Height: %g", width, height);
    std::shared_ptr<Drawing::RuntimeShaderBuilder> builder = GetWaveDisturbanceBuilder();
    if (!builder) {
        LOGE("GEWaveDisturbanceShaderMask::GenerateDrawingShader has builder error");
        return nullptr;
    }
    builder->SetUniform("clickPos", params_.clickPos[NUM_0], params_.clickPos[NUM_1]);
    builder->SetUniform("processTime", params_.progress);
    builder->SetUniform("waveLength", params_.waveLWH[NUM_0]);
    builder->SetUniform("waveWidth", params_.waveLWH[NUM_1]);
    builder->SetUniform("waveHeight", params_.waveLWH[NUM_2]);
    builder->SetUniform("waveDown", params_.waveRD[NUM_1]);
    builder->SetUniform("reflectRatio", params_.waveRD[NUM_0]);
    auto disturbanceShader = builder->MakeShader(nullptr, false);
    if (!disturbanceShader) {
        LOGE("GEWaveDisturbanceShaderMask::GenerateDrawingShader effect error");
    }
    return disturbanceShader;
}

std::shared_ptr<ShaderEffect> GEWaveDisturbanceShaderMask::GenerateDrawingShaderHasNormal(float width,
    float height) const
{
    // normal has no influence on the mask
    return GenerateDrawingShader(width, height);
}

std::shared_ptr<Drawing::RuntimeShaderBuilder> GEWaveDisturbanceShaderMask::GetWaveDisturbanceBuilder() const
{
    thread_local std::shared_ptr<Drawing::RuntimeShaderBuilder> waveDisturbanceBuilder = nullptr;
    if (waveDisturbanceBuilder) {
        return waveDisturbanceBuilder;
    }

    auto waveDisturbanceBuilderEffect = Drawing::RuntimeEffect::CreateForShader(WAVE_DISTURBANCE_PROG);
    if (!waveDisturbanceBuilderEffect) {
        LOGE("GEWaveDisturbanceShaderMask::GetWaveDisturbanceBuilder effect error");
        return nullptr;
    }

    waveDisturbanceBuilder = std::make_shared<Drawing::RuntimeShaderBuilder>(
        waveDisturbanceBuilderEffect);
    return waveDisturbanceBuilder;
}
} // Drawing
} // namespace Rosen
} // namespace OHOS
