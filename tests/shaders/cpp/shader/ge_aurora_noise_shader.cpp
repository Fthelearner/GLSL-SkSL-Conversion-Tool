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
#include "ge_aurora_noise_shader.h"
#include "ge_visual_effect_impl.h"

namespace OHOS {
namespace Rosen {

GEAuroraNoiseShader::GEAuroraNoiseShader() {}

GEAuroraNoiseShader::GEAuroraNoiseShader(Drawing::GEAuroraNoiseShaderParams& param)
{
    auroraNoiseParams_ = param;
}

void GEAuroraNoiseShader::MakeDrawingShader(const Drawing::Rect& rect, float progress)
{
    drShader_ = MakeAuroraNoiseShader(rect);
}

std::shared_ptr<Drawing::RuntimeShaderBuilder> GEAuroraNoiseShader::GetAuroraNoiseBuilder()
{
    thread_local std::shared_ptr<Drawing::RuntimeEffect> auroraNoiseShaderEffect_ = nullptr;

    if (auroraNoiseShaderEffect_ == nullptr) {
        static constexpr char prog[] = R"(
            uniform vec2 iResolution;
            uniform float noise;
            uniform float freqX;
            uniform float freqY;

            const float contrast = 4.64; // contrast constant, 464.0 / 100.0
            const float brightness = -0.17647; // brightness constant, -45.0 / 255.0

            float SNoise(in vec3 v);

            vec4 main(vec2 fragCoord)
            {
                vec2 uv = fragCoord / iResolution.xy;
                float aspect = iResolution.x / iResolution.y;
                vec2 p = vec2((uv.x / 0.75 - 1.0) * aspect, (uv.y * 2.0 - 1.0)); // horizontal stretch and map uv

                float innerFreqX = 2.0 * freqX;
                float innerFreqY = mix(2.0 * freqY, freqY, smoothstep(1.0, 0.0, uv.y));
                vec2 dom = vec2(p.x * innerFreqX, p.y * innerFreqY);

                float n = abs(SNoise(vec3(dom, noise * 3.0)));

                float alpha = clamp(1.0 - (n * contrast + brightness), 0.0, 1.0);
                return vec4(alpha);
            }

            vec3 Mod289(in vec3 x)
            {
                return x - floor(x * (1.0 / 289.0)) * 289.0; // 289.0: prime number for permutation
            }

            vec4 Mod289(in vec4 x)
            {
                return x - floor(x * (1.0 / 289.0)) * 289.0; // 289.0: prime number for permutation
            }

            vec4 Permute(in vec4 x)
            {
                return Mod289(((x * 34.0) + 1.0) * x); // 34.0: prime number for permutation
            }

            vec4 TaylorInvSqrt(in vec4 r)
            {
                return 1.79284291400159 - 0.85373472095314 * r; // Taylor series approximation for 1/sqrt(x)
            }

            float SNoise(in vec3 v)
            {
                const vec2 c = vec2(0.16666666666667, 0.33333333333333); // Constants for noise function, 1/6 and 1/3
                const vec4 d = vec4(0.0, 0.5, 1.0, 2.0); // Constants for noise function

                // First corner
                vec3 i = floor(v + dot(v, c.yyy));
                vec3 x0 = v - i + dot(i, c.xxx);

                // Other corners
                vec3 g = step(x0.yzx, x0.xyz);
                vec3 l = 1.0 - g;
                vec3 i1 = min(g.xyz, l.zxy);
                vec3 i2 = max(g.xyz, l.zxy);

                vec3 x1 = x0 - i1 + c.xxx;
                vec3 x2 = x0 - i2 + c.yyy; // 2.0 * c.x = 1/3 = c.y
                vec3 x3 = x0 - d.yyy; // -1.0 + 3.0 * c.x = -0.5 = -d.y

                // Permutations
                i = Mod289(i);
                vec4 p = Permute(Permute(Permute(
                    i.z + vec4(0.0, i1.z, i2.z, 1.0))
                    + i.y + vec4(0.0, i1.y, i2.y, 1.0))
                    + i.x + vec4(0.0, i1.x, i2.x, 1.0));

                float n = 0.142857142857; // i.e., 1.0 / 7.0
                vec3 ns = n * d.wyz - d.xzx;

                vec4 j = p - 49.0 * floor(p * ns.z * ns.z); // 49.0: mod(p, 7 * 7)

                vec4 xs = floor(j * ns.z);
                vec4 ys = floor(j - 7.0 * xs); // mod(j, N) // 7.0: mod(p, 7)

                vec4 x = xs *ns.x + ns.yyyy;
                vec4 y = ys *ns.x + ns.yyyy;
                vec4 h = 1.0 - abs(x) - abs(y);

                vec4 b0 = vec4(x.xy, y.xy);
                vec4 b1 = vec4(x.zw, y.zw);

                vec4 s0 = floor(b0) * 2.0 + 1.0; // 2.0: step size
                vec4 s1 = floor(b1) * 2.0 + 1.0; // 2.0: step size
                vec4 sh = -step(h, vec4(0.0));

                vec4 a0 = b0.xzyw + s0.xzyw * sh.xxyy ;
                vec4 a1 = b1.xzyw + s1.xzyw * sh.zzww ;

                vec3 p0 = vec3(a0.xy, h.x);
                vec3 p1 = vec3(a0.zw, h.y);
                vec3 p2 = vec3(a1.xy, h.z);
                vec3 p3 = vec3(a1.zw, h.w);

                //Normalise gradients
                vec4 norm = TaylorInvSqrt(vec4(dot(p0, p0), dot(p1, p1), dot(p2, p2), dot(p3, p3)));
                p0 *= norm.x;
                p1 *= norm.y;
                p2 *= norm.z;
                p3 *= norm.w;

                // Mix final noise value
                vec4 m = max(0.6 - vec4(dot(x0, x0), dot(x1, x1), dot(x2, x2), dot(x3, x3)), 0.0); // 0.6: falloff start
                m = m * m;
                return 42.0 * dot(m * m, vec4(dot(p0, x0), dot(p1, x1), dot(p2, x2), dot(p3, x3))); // scale to [-1,1]
            }
        )";
        auroraNoiseShaderEffect_ = Drawing::RuntimeEffect::CreateForShader(prog);
    }

    if (auroraNoiseShaderEffect_ == nullptr) {
        GE_LOGE("GEAuroraNoiseShader::GetAuroraNoiseBuilder auroraNoiseShaderEffect_ is nullptr.");
        return nullptr;
    }
    return std::make_shared<Drawing::RuntimeShaderBuilder>(auroraNoiseShaderEffect_);
}

std::shared_ptr<Drawing::RuntimeShaderBuilder> GEAuroraNoiseShader::GetAuroraNoiseVerticalBlurBuilder()
{
    thread_local std::shared_ptr<Drawing::RuntimeEffect> auroraNoiseVerticalBlurShaderEffect_ = nullptr;

    if (auroraNoiseVerticalBlurShaderEffect_ == nullptr) {
        static constexpr char prog[] = R"(
            uniform shader auroraNoiseTexture;
            uniform vec2 iResolution;

            const int maxSampleCount = 20;

            int CalculateDynamicSamples(float inDist, float maxDist) {
                float t = clamp(max(inDist, 0.0) / maxDist, 0.0, 1.0);
                float samples = mix(1.0, float(maxSampleCount), smoothstep(0.0, 1.0, t));
                return int(samples);
            }

            vec4 main(vec2 fragCoord)
            {
                vec2 uv = fragCoord / iResolution.xy;
                float dist = 1.2 - uv.y; // 1.2: origin height of the vertical blur
                float blurRadius = mix(0.0, 0.3, smoothstep(0.0, 1.2, dist)); // 0.3: blur radius on top, 1.2: origin

                vec4 col = vec4(0.0);
                float totalWeight = 0.0;
                int sampleCount = CalculateDynamicSamples(dist, 1.2); // 1.2: max sample distance
                for (int i = 0; i < maxSampleCount; ++i) {
                    if (i >= sampleCount) {
                        break;
                    }
                    float s = float(i) / float(sampleCount);
                    vec2 offset = vec2(0.0, s * blurRadius);
                    vec2 sampleUV = uv + offset;

                    sampleUV = clamp(sampleUV, vec2(0.0), vec2(1.0));
                    vec2 sampleCoord = sampleUV * iResolution.xy;
                    float weight = 1.0 - abs(s);
                    col += auroraNoiseTexture.eval(sampleCoord) * weight;
                    totalWeight += weight;
                }

                return col / totalWeight;
            }
        )";
        auroraNoiseVerticalBlurShaderEffect_ = Drawing::RuntimeEffect::CreateForShader(prog);
    }

    if (auroraNoiseVerticalBlurShaderEffect_ == nullptr) {
        GE_LOGE("GEAuroraNoiseShader::GetAuroraNoiseBuilder auroraNoiseVerticalBlurShaderEffect_ is nullptr.");
        return nullptr;
    }
    return std::make_shared<Drawing::RuntimeShaderBuilder>(auroraNoiseVerticalBlurShaderEffect_);
}

std::shared_ptr<Drawing::RuntimeShaderBuilder> GEAuroraNoiseShader::GetAuroraNoiseUpSamplingBuilder()
{
    thread_local std::shared_ptr<Drawing::RuntimeEffect> auroraNoiseUpSamplingShaderEffect_ = nullptr;

    if (auroraNoiseUpSamplingShaderEffect_ == nullptr) {
        static constexpr char prog[] = R"(
            uniform shader verticalBlurTexture;
            uniform vec2 iResolution;

            const float downSampleFactor = 8.0;

            vec4 main(vec2 fragCoord)
            {
                return verticalBlurTexture.eval(fragCoord / downSampleFactor);
            }
        )";
        auroraNoiseUpSamplingShaderEffect_ = Drawing::RuntimeEffect::CreateForShader(prog);
    }

    if (auroraNoiseUpSamplingShaderEffect_ == nullptr) {
        GE_LOGE("GEAuroraNoiseShader::GetAuroraNoiseBuilder auroraNoiseUpSamplingShaderEffect_ is nullptr.");
        return nullptr;
    }
    return std::make_shared<Drawing::RuntimeShaderBuilder>(auroraNoiseUpSamplingShaderEffect_);
}

void GEAuroraNoiseShader::Preprocess(Drawing::Canvas& canvas, const Drawing::Rect& rect)
{
    Drawing::Rect dsRect {0.0f, 0.0f, 0.125f * rect.GetWidth(), 0.125f * rect.GetHeight()}; // 0.125: 8x downSample
    Drawing::ImageInfo downSampledImg(std::ceil(dsRect.GetWidth()), dsRect.GetHeight(),
        Drawing::ColorType::COLORTYPE_RGBA_8888, Drawing::AlphaType::ALPHATYPE_OPAQUE);
    noiseImg_ = MakeAuroraNoiseGeneratorShader(canvas, downSampledImg);
    Drawing::ImageInfo verticalBlurImgInf(dsRect.GetWidth(), dsRect.GetHeight(),
        Drawing::ColorType::COLORTYPE_RGBA_8888, Drawing::AlphaType::ALPHATYPE_OPAQUE);
    verticalBlurImg_ = MakeAuroraNoiseVerticalBlurShader(canvas, verticalBlurImgInf);
}

std::shared_ptr<Drawing::Image> GEAuroraNoiseShader::MakeAuroraNoiseGeneratorShader(Drawing::Canvas& canvas,
    const Drawing::ImageInfo& imageInfo)
{
    float width = imageInfo.GetWidth();
    float height = imageInfo.GetHeight();
    builder_ = GetAuroraNoiseBuilder();
    if (builder_ == nullptr) {
        GE_LOGE("GEAuroraNoiseShader::MakeAuroraNoiseGeneratorShader builder_ is nullptr.");
        return nullptr;
    }
    builder_->SetUniform("iResolution", width, height);
    builder_->SetUniform("noise", auroraNoiseParams_.noise_);
    builder_->SetUniform("freqX", auroraNoiseParams_.freqX_);
    builder_->SetUniform("freqY", auroraNoiseParams_.freqY_);
    auto auroraNoiseGeneratorShader = builder_->MakeImage(canvas.GetGPUContext().get(), nullptr, imageInfo, false);
    if (auroraNoiseGeneratorShader == nullptr) {
        GE_LOGE("GEAuroraNoiseGeneratorShader auroraNoiseGeneratorShader is nullptr.");
        return nullptr;
    }
    return auroraNoiseGeneratorShader;
}

std::shared_ptr<Drawing::Image> GEAuroraNoiseShader::MakeAuroraNoiseVerticalBlurShader(Drawing::Canvas& canvas,
    const Drawing::ImageInfo& imageInfo)
{
    if (noiseImg_ == nullptr) {
        GE_LOGE("GEAuroraNoiseShader MakeAuroraNoiseVerticalBlurShader noiseImg_ is nullptr.");
        return nullptr;
    }
    float width = imageInfo.GetWidth();
    float height = imageInfo.GetHeight();
    verticalBlurBuilder_ = GetAuroraNoiseVerticalBlurBuilder();
    if (verticalBlurBuilder_ == nullptr) {
        GE_LOGE("GEAuroraNoiseShader::MakeAuroraNoiseShader verticalBlurBuilder_ is nullptr.");
        return nullptr;
    }
    Drawing::Matrix matrix;
    auto auroraNoiseShader = Drawing::ShaderEffect::CreateImageShader(*noiseImg_,
        Drawing::TileMode::CLAMP, Drawing::TileMode::CLAMP,
        Drawing::SamplingOptions(Drawing::FilterMode::LINEAR), matrix);
    verticalBlurBuilder_->SetChild("auroraNoiseTexture", auroraNoiseShader);
    verticalBlurBuilder_->SetUniform("iResolution", width, height);
    auto auroraNoiseVerticleBlurShader =
        verticalBlurBuilder_->MakeImage(canvas.GetGPUContext().get(), nullptr, imageInfo, false);
    if (auroraNoiseVerticleBlurShader == nullptr) {
        GE_LOGE("GEAuroraNoiseShader::MakeAuroraNoiseShader auroraNoiseVerticleBlurShader is nullptr.");
        return nullptr;
    }
    return auroraNoiseVerticleBlurShader;
}

std::shared_ptr<Drawing::ShaderEffect> GEAuroraNoiseShader::MakeAuroraNoiseShader(const Drawing::Rect& rect)
{
    if (verticalBlurImg_ == nullptr) {
        GE_LOGE("GEAuroraNoiseShader MakeAuroraNoiseShader verticalBlurImg_ is nullptr.");
        return nullptr;
    }
    if (!rect.IsValid()) {
        GE_LOGE("GEAuroraNoiseShader::MakeAuroraNoiseShader rect is invalid.");
        return nullptr;
    }
    auto width = rect.GetWidth();
    auto height = rect.GetHeight();
    Drawing::Matrix matrix;
    auto verticalBlurShader = Drawing::ShaderEffect::CreateImageShader(*verticalBlurImg_, Drawing::TileMode::CLAMP,
        Drawing::TileMode::CLAMP, Drawing::SamplingOptions(Drawing::FilterMode::LINEAR), matrix);
    upSamplingBuilder_ = GetAuroraNoiseUpSamplingBuilder();
    if (upSamplingBuilder_ == nullptr) {
        GE_LOGE("GEAuroraNoiseShader::MakeAuroraNoiseShader upSamplingBuilder_ is nullptr.");
        return nullptr;
    }
    upSamplingBuilder_->SetChild("verticalBlurTexture", verticalBlurShader);
    upSamplingBuilder_->SetUniform("iResolution", width, height);
    auto auroraNoiseUpSamplingShader = upSamplingBuilder_->MakeShader(nullptr, false);
    if (auroraNoiseUpSamplingShader == nullptr) {
        GE_LOGE("GEAuroraNoiseShader::MakeAuroraNoiseShader auroraNoiseUpSamplingShader is nullptr.");
        return nullptr;
    }
    return auroraNoiseUpSamplingShader;
}
} // namespace Rosen
} // namespace OHOS